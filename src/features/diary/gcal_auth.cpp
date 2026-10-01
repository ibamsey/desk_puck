#include "diary/gcal_auth.h"

#include "diary/diary_sync.h"
#include "diary/gcal_http.h"
#include "gcal/gcal_secrets.h"
#include "time/wall_clock.h"
#include "wifi/wifi_station.h"

#include <Arduino.h>

#include <ArduinoJson.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

static const char* kScopeEncoded =
    "https%3A%2F%2Fwww.googleapis.com%2Fauth%2Fcalendar.readonly";
static const char* kDeviceCodeUrl = "https://oauth2.googleapis.com/device/code";
static const char* kTokenUrl = "https://oauth2.googleapis.com/token";

static GcalLinkState s_link_state = GcalLinkState::NotConfigured;
static char s_user_code[32] = {};
static char s_qr_url[160] = {};
static char s_status[64] = {};
static char s_device_code[256] = {};
static unsigned long s_device_code_expires_ms = 0;
static unsigned long s_next_poll_ms = 0;
static unsigned long s_poll_interval_ms = 5000;

static char s_refresh_token[512] = {};
static char s_access_token[2048] = {};
static unsigned long s_access_expires_ms = 0;

static bool s_http_busy = false;
static enum class HttpJob { None, DeviceCode, TokenPoll, Refresh } s_job = HttpJob::None;

static void set_status(const char* msg) {
    strncpy(s_status, msg ? msg : "", sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
}

static void report_device_code_failure(int http_code, const String& resp) {
    StaticJsonDocument<384> doc;
    if (resp.length() > 0 && !deserializeJson(doc, resp)) {
        const char* err = doc["error"];
        const char* desc = doc["error_description"];
        if (err) {
            Serial.printf("[GCAL] device/code OAuth error: %s", err);
            if (desc) {
                Serial.printf(" (%s)", desc);
            }
            Serial.println();
            if (strcmp(err, "invalid_client") == 0) {
                set_status("Use TV OAuth client");
                return;
            }
            if (desc && desc[0]) {
                char buf[sizeof(s_status)];
                snprintf(buf, sizeof(buf), "%.48s", desc);
                set_status(buf);
                return;
            }
            set_status(err);
            return;
        }
    }
    if (http_code <= 0) {
        set_status("HTTPS failed");
        Serial.printf("[GCAL] device/code connect failed (code %d). "
                      "Check WiFi and wait for [TIME] synced before Connect.\n",
                      http_code);
        return;
    }
    set_status("Google HTTP error");
    Serial.printf("[GCAL] device/code HTTP %d: %.180s\n", http_code, resp.c_str());
}

static void load_refresh_nvs() {
    Preferences p;
    if (p.begin("gcal", true)) {
        String r = p.getString("refresh", "");
        strncpy(s_refresh_token, r.c_str(), sizeof(s_refresh_token) - 1);
        s_refresh_token[sizeof(s_refresh_token) - 1] = '\0';
        p.end();
    }
}

static void save_refresh_nvs() {
    Preferences p;
    if (p.begin("gcal", false)) {
        p.putString("refresh", s_refresh_token);
        p.putUChar("linked", 1);
        p.end();
    }
}

static void clear_refresh_nvs() {
    Preferences p;
    if (p.begin("gcal", false)) {
        p.remove("refresh");
        p.putUChar("linked", 0);
        p.end();
    }
    s_refresh_token[0] = '\0';
    s_access_token[0] = '\0';
}

static bool client_is_placeholder() {
    return strstr(GCAL_CLIENT_ID, "REPLACE_ME") != nullptr ||
           strstr(GCAL_CLIENT_ID, "your-client-id") != nullptr;
}

static bool client_configured() {
    return GCAL_CLIENT_ID[0] != '\0' && !client_is_placeholder();
}

static void build_qr_url(const char* verification_url, const char* user_code) {
    snprintf(s_qr_url, sizeof(s_qr_url), "%s?user_code=%s", verification_url, user_code);
}

static void handle_token_response(const String& body, bool from_device_poll) {
    StaticJsonDocument<1536> doc;
    if (deserializeJson(doc, body)) {
        set_status("token parse err");
        s_link_state = GcalLinkState::LinkFailed;
        return;
    }
    if (doc.containsKey("error")) {
        const char* err = doc["error"];
        if (strcmp(err, "authorization_pending") == 0) {
            s_link_state = GcalLinkState::PollingToken;
            set_status("Waiting for Google...");
            return;
        }
        if (strcmp(err, "slow_down") == 0) {
            s_poll_interval_ms += 5000;
            s_link_state = GcalLinkState::PollingToken;
            return;
        }
        if (strcmp(err, "access_denied") == 0) {
            set_status("Access denied");
            s_link_state = GcalLinkState::PromptConnect;
            return;
        }
        if (strcmp(err, "expired_token") == 0) {
            set_status("Code expired");
            s_link_state = GcalLinkState::PromptConnect;
            return;
        }
        if (strcmp(err, "invalid_grant") == 0) {
            clear_refresh_nvs();
            set_status("Link expired");
            s_link_state = GcalLinkState::NeedsReauth;
            return;
        }
        set_status(err);
        s_link_state = GcalLinkState::LinkFailed;
        return;
    }

    const char* access = doc["access_token"];
    if (!access) {
        set_status("no access_token");
        s_link_state = GcalLinkState::LinkFailed;
        return;
    }
    strncpy(s_access_token, access, sizeof(s_access_token) - 1);
    s_access_token[sizeof(s_access_token) - 1] = '\0';
    const int expires_in = doc["expires_in"] | 3600;
    s_access_expires_ms = millis() + (unsigned long)expires_in * 1000UL - 60000UL;

    if (from_device_poll) {
        const char* refresh = doc["refresh_token"];
        if (refresh && refresh[0]) {
            strncpy(s_refresh_token, refresh, sizeof(s_refresh_token) - 1);
            s_refresh_token[sizeof(s_refresh_token) - 1] = '\0';
            save_refresh_nvs();
        }
        s_link_state = GcalLinkState::Linked;
        set_status("");
        Serial.println("[GCAL] linked OK");
        diary_sync_request();
    }
}

void gcal_auth_begin() {
    load_refresh_nvs();
    if (s_refresh_token[0]) {
        s_link_state = GcalLinkState::Linked;
        set_status("");
    } else if (!client_configured()) {
        s_link_state = GcalLinkState::NotConfigured;
        if (client_is_placeholder()) {
            set_status("Save gcal_config_private.h");
            Serial.println("[GCAL] OAuth still placeholder — save real Client ID in "
                           "include/gcal/gcal_config_private.h and reflash");
        } else {
            set_status("Missing OAuth config");
        }
    } else {
        s_link_state = GcalLinkState::PromptConnect;
        set_status("");
    }
}

void gcal_auth_request_connect() {
    if (!client_configured()) {
        s_link_state = GcalLinkState::NotConfigured;
        return;
    }
    if (!wifi_station_is_connected()) {
        set_status("WiFi required");
        s_link_state = GcalLinkState::PromptConnect;
        return;
    }
    s_device_code[0] = '\0';
    s_user_code[0] = '\0';
    s_qr_url[0] = '\0';
    s_link_state = GcalLinkState::DeviceCodePending;
    s_job = HttpJob::DeviceCode;
    s_http_busy = false;
    set_status("Requesting code...");
}

void gcal_auth_cancel_connect() {
    if (s_link_state == GcalLinkState::PollingToken || s_link_state == GcalLinkState::ShowQr ||
        s_link_state == GcalLinkState::DeviceCodePending) {
        s_link_state = s_refresh_token[0] ? GcalLinkState::Linked : GcalLinkState::PromptConnect;
        s_job = HttpJob::None;
        set_status("");
    }
}

void gcal_auth_clear_link() {
    clear_refresh_nvs();
    s_link_state = GcalLinkState::PromptConnect;
    set_status("");
}

GcalLinkState gcal_auth_link_state() {
    return s_link_state;
}

bool gcal_auth_is_linked() {
    return s_refresh_token[0] != '\0' && s_link_state != GcalLinkState::NeedsReauth;
}

const char* gcal_auth_qr_url() {
    return s_qr_url;
}

const char* gcal_auth_user_code() {
    return s_user_code;
}

const char* gcal_auth_status_message() {
    return s_status;
}

const char* gcal_auth_access_token() {
    if (!s_refresh_token[0]) {
        return nullptr;
    }
    if (s_access_token[0] && millis() < s_access_expires_ms) {
        return s_access_token;
    }
    if (s_job == HttpJob::Refresh) {
        return nullptr;
    }
    s_job = HttpJob::Refresh;
    s_http_busy = false;
    return nullptr;
}

void gcal_auth_poll() {
    const unsigned long now = millis();

    if (s_link_state == GcalLinkState::DeviceCodePending && s_job == HttpJob::None) {
        s_job = HttpJob::DeviceCode;
    }

    if (s_link_state == GcalLinkState::PollingToken && now >= s_next_poll_ms && !s_http_busy &&
        s_job == HttpJob::None) {
        s_job = HttpJob::TokenPoll;
    }

    if (s_job == HttpJob::Refresh && !s_http_busy) {
        s_http_busy = true;
        char body[640];
        snprintf(body, sizeof(body),
                 "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",
                 GCAL_CLIENT_ID, GCAL_CLIENT_SECRET, s_refresh_token);
        String resp;
        int code = 0;
        if (gcal_http::post_form(kTokenUrl, body, resp, code)) {
            handle_token_response(resp, false);
        } else {
            set_status("refresh HTTP fail");
        }
        s_job = HttpJob::None;
        s_http_busy = false;
        return;
    }

    if (s_job == HttpJob::DeviceCode && !s_http_busy) {
        if (!wall_clock_is_synced()) {
            set_status("Syncing time...");
            return;
        }
        s_http_busy = true;
        char body[384];
        snprintf(body, sizeof(body), "client_id=%s&scope=%s", GCAL_CLIENT_ID, kScopeEncoded);
        String resp;
        int http_code = 0;
        if (!gcal_http::post_form(kDeviceCodeUrl, body, resp, http_code) || http_code != 200) {
            report_device_code_failure(http_code, resp);
            s_link_state = GcalLinkState::LinkFailed;
            s_job = HttpJob::None;
            s_http_busy = false;
            return;
        }
        StaticJsonDocument<768> doc;
        if (deserializeJson(doc, resp)) {
            set_status("device JSON err");
            s_link_state = GcalLinkState::LinkFailed;
            s_job = HttpJob::None;
            s_http_busy = false;
            return;
        }
        const char* dc = doc["device_code"];
        const char* uc = doc["user_code"];
        const char* vu = doc["verification_url"];
        const int expires_in = doc["expires_in"] | 900;
        const int interval = doc["interval"] | 5;
        if (!dc || !uc || !vu) {
            set_status("device fields missing");
            s_link_state = GcalLinkState::LinkFailed;
            s_job = HttpJob::None;
            s_http_busy = false;
            return;
        }
        strncpy(s_device_code, dc, sizeof(s_device_code) - 1);
        strncpy(s_user_code, uc, sizeof(s_user_code) - 1);
        build_qr_url(vu, uc);
        s_device_code_expires_ms = now + (unsigned long)expires_in * 1000UL;
        s_poll_interval_ms = (unsigned long)interval * 1000UL;
        s_next_poll_ms = now + s_poll_interval_ms;
        s_link_state = GcalLinkState::ShowQr;
        set_status("Scan QR with phone");
        s_job = HttpJob::None;
        s_http_busy = false;
        s_link_state = GcalLinkState::PollingToken;
        s_job = HttpJob::TokenPoll;
        return;
    }

    if (s_job == HttpJob::TokenPoll && !s_http_busy) {
        if (now > s_device_code_expires_ms && s_device_code[0]) {
            set_status("Code expired");
            s_link_state = GcalLinkState::PromptConnect;
            s_job = HttpJob::None;
            return;
        }
        s_http_busy = true;
        char body[768];
        snprintf(body, sizeof(body),
                 "client_id=%s&client_secret=%s&device_code=%s&grant_type=%s", GCAL_CLIENT_ID,
                 GCAL_CLIENT_SECRET, s_device_code, "urn:ietf:params:oauth:grant-type:device_code");
        String resp;
        int http_code = 0;
        if (!gcal_http::post_form(kTokenUrl, body, resp, http_code)) {
            set_status("poll HTTP fail");
            s_http_busy = false;
            s_job = HttpJob::None;
            s_next_poll_ms = now + s_poll_interval_ms;
            return;
        }
        handle_token_response(resp, true);
        s_http_busy = false;
        s_job = HttpJob::None;
        if (s_link_state == GcalLinkState::PollingToken) {
            s_next_poll_ms = now + s_poll_interval_ms;
        } else {
            s_device_code[0] = '\0';
        }
    }
}

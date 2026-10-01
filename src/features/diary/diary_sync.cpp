#include "diary/diary_sync.h"

#include <Arduino.h>

#include "diary/diary_cache.h"
#include "diary/gcal_auth.h"
#include "diary/gcal_http.h"
#include "time/wall_clock.h"
#include "wifi/wifi_station.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DiaryDayBuffer s_day;
static DiarySyncState s_state = DiarySyncState::Idle;
static char s_error[64] = {};
static unsigned long s_last_ok_ms = 0;
static unsigned long s_last_fail_ms = 0;
static bool s_fetch_pending = false;

static const unsigned long kRetryAfterFailMs = 5UL * 60UL * 1000UL;

static const unsigned long kStaleMs = 15UL * 60UL * 1000UL;

static void set_error(const char* e) {
    strncpy(s_error, e ? e : "", sizeof(s_error) - 1);
    s_state = DiarySyncState::Error;
}

static int parse_minutes_from_local_iso(const char* iso) {
    if (!iso || strlen(iso) < 16) {
        return 0;
    }
    int h = 0, m = 0;
    if (iso[10] == 'T') {
        sscanf(iso + 11, "%d:%d", &h, &m);
    }
    return h * 60 + m;
}

static void url_encode(const char* in, char* out, size_t out_len) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 4 < out_len; ++i) {
        const char c = in[i];
        if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[j++] = c;
        } else if (c == ' ') {
            out[j++] = '+';
        } else {
            j += (size_t)snprintf(out + j, out_len - j, "%%%02X", (unsigned char)c);
        }
    }
    out[j] = '\0';
}

void diary_sync_begin() {
    diary_cache_begin();
    if (diary_cache_load(s_day)) {
        s_last_ok_ms = millis();
    }
    s_state = DiarySyncState::Idle;
}

void diary_sync_request() {
    if (!gcal_auth_is_linked()) {
        return;
    }
    if (s_last_fail_ms != 0 && millis() - s_last_fail_ms < kRetryAfterFailMs) {
        return;
    }
    s_fetch_pending = true;
}

void diary_sync_poll() {
    if (!gcal_auth_is_linked()) {
        s_fetch_pending = false;
        return;
    }

    if (!s_fetch_pending && s_state == DiarySyncState::Idle) {
        return;
    }

    if (!wifi_station_is_connected()) {
        set_error("offline");
        s_fetch_pending = false;
        return;
    }
    if (!wall_clock_is_synced()) {
        s_state = DiarySyncState::WaitingToken;
        return;
    }

    const char* token = gcal_auth_access_token();
    if (!token) {
        s_state = DiarySyncState::WaitingToken;
        gcal_auth_poll();
        return;
    }

    s_state = DiarySyncState::Fetching;
    s_error[0] = '\0';

    char tmin[40];
    char tmax[40];
    if (!wall_clock_today_rfc3339_bounds(tmin, sizeof(tmin), tmax, sizeof(tmax))) {
        set_error("time bounds");
        s_fetch_pending = false;
        return;
    }

    char enc_min[80];
    char enc_max[80];
    url_encode(tmin, enc_min, sizeof(enc_min));
    url_encode(tmax, enc_max, sizeof(enc_max));

    char url[512];
    snprintf(url, sizeof(url),
             "https://www.googleapis.com/calendar/v3/calendars/primary/events?"
             "timeMin=%s&timeMax=%s&singleEvents=true&orderBy=startTime&maxResults=50",
             enc_min, enc_max);

#if defined(DEBUG_GCAL) && DEBUG_GCAL
    Serial.printf("[DIARY] GET bounds %s .. %s\n", tmin, tmax);
#endif

    String body;
    int code = 0;
    if (!gcal_http::get_bearer(url, token, body, code)) {
        Serial.println("[DIARY] calendar GET failed (TLS/HTTP)");
        set_error("HTTP fail");
        s_fetch_pending = false;
        s_state = DiarySyncState::Idle;
        s_last_fail_ms = millis();
        return;
    }
    if (code == 401) {
        Serial.println("[DIARY] calendar GET 401");
        set_error("auth expired");
        s_fetch_pending = false;
        s_state = DiarySyncState::Idle;
        s_last_fail_ms = millis();
        return;
    }
    if (code != 200) {
        Serial.printf("[DIARY] calendar GET %d: %.240s\n", code, body.c_str());
        set_error("calendar err");
        s_fetch_pending = false;
        s_state = DiarySyncState::Idle;
        s_last_fail_ms = millis();
        return;
    }

    StaticJsonDocument<16384> doc;
    if (deserializeJson(doc, body)) {
        Serial.printf("[DIARY] JSON parse fail (len=%u)\n", (unsigned)body.length());
        set_error("JSON err");
        s_fetch_pending = false;
        s_state = DiarySyncState::Idle;
        s_last_fail_ms = millis();
        return;
    }

    wall_clock_now_ymd(s_day.year, s_day.month, s_day.day);
    s_day.clear();

    JsonArray items = doc["items"].as<JsonArray>();
    for (JsonObject item : items) {
        if (s_day.count >= DIARY_MAX_EVENTS) {
            break;
        }
        const char* status = item["status"];
        if (status && strcmp(status, "cancelled") == 0) {
            continue;
        }
        DiaryEventBuf& ev = s_day.events[s_day.count];
        const char* summary = item["summary"] | "(no title)";
        strncpy(ev.title, summary, DIARY_TITLE_LEN);
        ev.title[DIARY_TITLE_LEN] = '\0';

        JsonObject start = item["start"];
        if (start["date"].is<const char*>()) {
            ev.all_day = true;
            ev.start_min = 0;
            ev.end_min = 24 * 60;
        } else {
            ev.all_day = false;
            const char* dt = start["dateTime"];
            ev.start_min = (int16_t)parse_minutes_from_local_iso(dt);
            JsonObject end = item["end"];
            const char* et = end["dateTime"];
            ev.end_min = (int16_t)parse_minutes_from_local_iso(et);
        }
        s_day.count++;
    }

    diary_cache_save(s_day);
    s_last_ok_ms = millis();
    s_last_fail_ms = 0;
    s_state = DiarySyncState::Done;
    s_fetch_pending = false;
    Serial.printf("[DIARY] synced %u events\n", (unsigned)s_day.count);
    s_state = DiarySyncState::Idle;
}

DiarySyncState diary_sync_state() {
    return s_state;
}

const char* diary_sync_last_error() {
    return s_error;
}

bool diary_sync_busy() {
    return s_fetch_pending || s_state == DiarySyncState::Fetching ||
           s_state == DiarySyncState::WaitingToken;
}

const DiaryDayBuffer& diary_sync_day() {
    return s_day;
}

unsigned long diary_sync_last_ok_ms() {
    return s_last_ok_ms;
}

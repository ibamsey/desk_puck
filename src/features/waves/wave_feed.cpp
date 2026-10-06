#include "waves/wave_feed.h"

#include "tide/tide_feed.h"
#include "tide/tide_secrets.h"
#include "wifi/wifi_station.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <stdio.h>
#include <string.h>

static const unsigned long kRefreshMs = 15UL * 60UL * 1000UL;
static const unsigned long kRetryAfterFailMs = 3UL * 60UL * 1000UL;
static const unsigned long kWifiSettleMs = 800;

static WaveFeedSnapshot s_snap;
static unsigned long s_last_fetch_ms = 0;
static unsigned long s_last_fail_ms = 0;
static unsigned long s_wifi_up_ms = 0;
static bool s_force_refresh = true;
static bool s_fetch_pending = false;

static WiFiClientSecure s_http_client;

static void clear_error() {
    s_snap.error[0] = '\0';
}

static void set_error(const char* msg) {
    strncpy(s_snap.error, msg ? msg : "error", sizeof(s_snap.error) - 1);
    s_snap.error[sizeof(s_snap.error) - 1] = '\0';
    s_last_fail_ms = millis();
    s_snap.loading = false;
    s_fetch_pending = false;
}

static bool wifi_has_ip() {
    return wifi_station_is_connected() && WiFi.localIP()[0] != 0;
}

static float parse_prop_float(JsonObject props, const char* key) {
    if (props[key].is<const char*>()) {
        return atof(props[key].as<const char*>());
    }
    return props[key].as<float>();
}

static bool parse_wave_body(const String& body) {
    StaticJsonDocument<4096> doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        set_error("json");
        Serial.printf("[WAVE] json: %s\n", err.c_str());
        return false;
    }

    JsonArray features = doc["features"].as<JsonArray>();
    if (features.isNull() || features.size() == 0) {
        set_error("no data");
        return false;
    }

    JsonObject props = features[0]["properties"];
    if (props.isNull()) {
        set_error("no props");
        return false;
    }

    const float hs = parse_prop_float(props, "hs");
    const float tp = parse_prop_float(props, "tp");
    const float tz = parse_prop_float(props, "tz");
    if (hs < 0.0f || tp <= 0.05f) {
        set_error("bad values");
        return false;
    }

    s_snap.hs_m = hs;
    s_snap.tp_s = tp;
    s_snap.tz_s = tz > 0.05f ? tz : tp;
    s_snap.sample_epoch = 0;
    s_snap.valid = true;
    s_snap.loading = false;
    clear_error();
    s_last_fetch_ms = millis();
    s_fetch_pending = false;
    Serial.printf("[WAVE] hs=%.2f tp=%.2f tz=%.2f heap=%u\n", (double)hs, (double)tp, (double)s_snap.tz_s,
                  (unsigned)ESP.getFreeHeap());
    return true;
}

static bool http_get_waves(String& body_out, int& code_out) {
    static bool tls_inited = false;
    if (!tls_inited) {
        s_http_client.setInsecure();
        tls_inited = true;
    }

    char url[256];
    snprintf(url, sizeof(url),
             "https://coastalmonitoring.org/observations/waves/latest.geojson?key=%s&sensor=%s&duration=1",
             TIDE_CCO_API_KEY, WAVE_SENSOR_ID);

    HTTPClient http;
    if (!http.begin(s_http_client, url)) {
        set_error("http begin");
        return false;
    }
    http.setTimeout(20000);
    http.addHeader("Referer", TIDE_CCO_REFERER);
    http.setReuse(false);
    code_out = http.GET();
    if (code_out > 0) {
        body_out = http.getString();
    } else {
        body_out = "";
    }
    http.end();
    return code_out == HTTP_CODE_OK;
}

static bool fetch_waves() {
    String body;
    int code = 0;
    if (!http_get_waves(body, code)) {
        set_error("http");
        Serial.printf("[WAVE] GET %d heap=%u\n", code, (unsigned)ESP.getFreeHeap());
        return false;
    }
    const bool ok = parse_wave_body(body);
    body = String();
    return ok;
}

static bool should_refresh(unsigned long now) {
    if (s_force_refresh) {
        return true;
    }
    if (!s_snap.valid) {
        return true;
    }
    if (s_last_fetch_ms == 0) {
        return true;
    }
    return (now - s_last_fetch_ms) >= kRefreshMs;
}

void wave_feed_begin() {
    s_force_refresh = true;
    s_fetch_pending = false;
    s_snap = WaveFeedSnapshot{};
}

void wave_feed_poll() {
    const unsigned long now = millis();

    if (s_snap.valid && s_last_fetch_ms != 0) {
        s_snap.fetched_age_ms = now - s_last_fetch_ms;
    }

    if (!wifi_has_ip()) {
        s_wifi_up_ms = 0;
        return;
    }

    if (s_wifi_up_ms == 0) {
        s_wifi_up_ms = now;
    }
    if (now - s_wifi_up_ms < kWifiSettleMs) {
        return;
    }

    if (tide_feed_is_busy()) {
        return;
    }

    if (s_fetch_pending) {
        fetch_waves();
        return;
    }

    if (s_last_fail_ms != 0 && now - s_last_fail_ms < kRetryAfterFailMs && !s_force_refresh) {
        return;
    }
    if (!should_refresh(now)) {
        return;
    }

    s_force_refresh = false;
    s_snap.loading = true;
    s_fetch_pending = true;
}

void wave_feed_snapshot(WaveFeedSnapshot& out) {
    out = s_snap;
    if (out.valid && s_last_fetch_ms != 0) {
        out.fetched_age_ms = millis() - s_last_fetch_ms;
    }
}

bool wave_feed_is_ready() {
    return s_snap.valid;
}

void wave_feed_request_refresh() {
    s_force_refresh = true;
    s_last_fail_ms = 0;
    s_fetch_pending = true;
    if (wifi_has_ip()) {
        s_snap.loading = true;
    }
}

const char* wave_feed_last_error() {
    return s_snap.error;
}

#include "weather/local_weather.h"

#include "config.h"
#include "wifi/wifi_station.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef WEATHER_DEFAULT_LAT
#define WEATHER_DEFAULT_LAT 51.5074f
#endif
#ifndef WEATHER_DEFAULT_LON
#define WEATHER_DEFAULT_LON -0.1278f
#endif

static const unsigned long kRefreshMs = 30UL * 60UL * 1000UL;
static const unsigned long kRetryAfterFailMs = 5UL * 60UL * 1000UL;
static const unsigned long kWifiSettleMs = 800;

static float s_lat = WEATHER_DEFAULT_LAT;
static float s_lon = WEATHER_DEFAULT_LON;
static bool s_location_from_nvs = false;

static LocalWeatherNow s_now;
static unsigned long s_last_fetch_ms = 0;
static unsigned long s_last_fail_ms = 0;
static unsigned long s_wifi_up_ms = 0;
static bool s_force_refresh = false;
static char s_error[48] = {};

static WiFiClientSecure s_http_client;

static void clear_error() {
    s_error[0] = '\0';
}

static void set_error(const char* msg) {
    strncpy(s_error, msg ? msg : "error", sizeof(s_error) - 1);
    s_error[sizeof(s_error) - 1] = '\0';
    s_last_fail_ms = millis();
}

static bool wifi_has_ip() {
    return wifi_station_is_connected() && WiFi.localIP()[0] != 0;
}

static void summary_from_code(int code, char* out, size_t out_len) {
    const char* label = "Unknown";
    switch (code) {
    case 0:
        label = "Clear";
        break;
    case 1:
    case 2:
    case 3:
        label = "Cloudy";
        break;
    case 45:
    case 48:
        label = "Fog";
        break;
    case 51:
    case 53:
    case 55:
        label = "Drizzle";
        break;
    case 56:
    case 57:
        label = "Freezing drizzle";
        break;
    case 61:
    case 63:
    case 65:
        label = "Rain";
        break;
    case 66:
    case 67:
        label = "Freezing rain";
        break;
    case 71:
    case 73:
    case 75:
        label = "Snow";
        break;
    case 77:
        label = "Snow grains";
        break;
    case 80:
    case 81:
    case 82:
        label = "Showers";
        break;
    case 85:
    case 86:
        label = "Snow showers";
        break;
    case 95:
        label = "Thunderstorm";
        break;
    case 96:
    case 99:
        label = "Hail storm";
        break;
    default:
        break;
    }
    strncpy(out, label, out_len - 1);
    out[out_len - 1] = '\0';
}

static void load_location_nvs() {
    Preferences prefs;
    if (!prefs.begin("weather", true)) {
        return;
    }
    if (prefs.isKey("lat") && prefs.isKey("lon")) {
        s_lat = prefs.getFloat("lat", WEATHER_DEFAULT_LAT);
        s_lon = prefs.getFloat("lon", WEATHER_DEFAULT_LON);
        s_location_from_nvs = true;
    }
    prefs.end();
}

static bool save_location_nvs() {
    Preferences prefs;
    if (!prefs.begin("weather", false)) {
        return false;
    }
    prefs.putFloat("lat", s_lat);
    prefs.putFloat("lon", s_lon);
    prefs.end();
    s_location_from_nvs = true;
    return true;
}

static bool load_snapshot_nvs() {
    Preferences prefs;
    if (!prefs.begin("weather", true)) {
        return false;
    }
    const size_t len = prefs.getBytesLength("snap");
    if (len != sizeof(LocalWeatherNow)) {
        prefs.end();
        return false;
    }
    LocalWeatherNow loaded;
    prefs.getBytes("snap", &loaded, sizeof(loaded));
    prefs.end();
    if (!loaded.valid) {
        return false;
    }
    s_now = loaded;
    s_now.fetched_age_ms = 0;
    return true;
}

static void save_snapshot_nvs() {
    if (!s_now.valid) {
        return;
    }
    Preferences prefs;
    if (!prefs.begin("weather", false)) {
        return;
    }
    prefs.putBytes("snap", &s_now, sizeof(s_now));
    prefs.end();
}

static bool fetch_open_meteo() {
    static bool tls_inited = false;
    if (!tls_inited) {
        s_http_client.setInsecure();
        tls_inited = true;
    }

    char url[320];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?"
             "latitude=%.4f&longitude=%.4f&"
             "current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&"
             "wind_speed_unit=kmh&timezone=auto",
             (double)s_lat, (double)s_lon);

    HTTPClient http;
    if (!http.begin(s_http_client, url)) {
        set_error("http begin");
        return false;
    }
    http.setTimeout(15000);
    const int code = http.GET();
    String body;
    if (code > 0) {
        body = http.getString();
    }
    http.end();

    if (code != HTTP_CODE_OK) {
        set_error("http status");
        Serial.printf("[WEATHER] GET %d\n", code);
        return false;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        set_error("json");
        return false;
    }

    JsonObject cur = doc["current"];
    if (cur.isNull()) {
        set_error("no current");
        return false;
    }

    LocalWeatherNow next;
    next.valid = true;
    next.temp_c = (int16_t)lround(cur["temperature_2m"].as<float>());
    next.humidity_pct = (uint8_t)cur["relative_humidity_2m"].as<int>();
    next.wind_kmh = (uint16_t)lround(cur["wind_speed_10m"].as<float>());
    next.weather_code = (int16_t)cur["weather_code"].as<int>();
    summary_from_code(next.weather_code, next.summary, sizeof(next.summary));
    next.fetched_age_ms = 0;

    s_now = next;
    s_last_fetch_ms = millis();
    clear_error();
    save_snapshot_nvs();
    Serial.printf("[WEATHER] ok %dC %s hum=%u wind=%u\n", (int)s_now.temp_c, s_now.summary,
                  (unsigned)s_now.humidity_pct, (unsigned)s_now.wind_kmh);
    return true;
}

static bool should_fetch(unsigned long now) {
    if (s_force_refresh) {
        return true;
    }
    if (!s_now.valid) {
        return true;
    }
    if (s_last_fetch_ms == 0) {
        return true;
    }
    if (now - s_last_fetch_ms >= kRefreshMs) {
        return true;
    }
    return false;
}

void local_weather_begin() {
    load_location_nvs();
    load_snapshot_nvs();
    s_force_refresh = !s_now.valid;
}

void local_weather_poll() {
    const unsigned long now = millis();

    if (s_now.valid && s_last_fetch_ms != 0) {
        s_now.fetched_age_ms = now - s_last_fetch_ms;
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

    if (s_last_fail_ms != 0 && now - s_last_fail_ms < kRetryAfterFailMs && !s_force_refresh) {
        return;
    }

    if (!should_fetch(now)) {
        return;
    }

    s_force_refresh = false;
    if (!fetch_open_meteo()) {
        Serial.printf("[WEATHER] fail: %s\n", s_error);
    }
}

bool local_weather_has_location() {
    return s_location_from_nvs || (fabsf(s_lat) <= 90.0f && fabsf(s_lon) <= 180.0f);
}

bool local_weather_get_location(float& lat, float& lon) {
    lat = s_lat;
    lon = s_lon;
    return local_weather_has_location();
}

bool local_weather_set_location(float lat, float lon) {
    if (fabsf(lat) > 90.0f || fabsf(lon) > 180.0f) {
        return false;
    }
    s_lat = lat;
    s_lon = lon;
    if (!save_location_nvs()) {
        return false;
    }
    local_weather_request_refresh();
    return true;
}

void local_weather_now(LocalWeatherNow& out) {
    out = s_now;
    if (out.valid && s_last_fetch_ms != 0) {
        out.fetched_age_ms = millis() - s_last_fetch_ms;
    }
}

bool local_weather_is_ready() {
    return s_now.valid;
}

void local_weather_request_refresh() {
    s_force_refresh = true;
}

const char* local_weather_last_error() {
    return s_error;
}

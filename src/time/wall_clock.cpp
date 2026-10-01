#include "time/wall_clock.h"

#include "config.h"
#include "wifi/wifi_station.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>

#ifndef WALL_CLOCK_DEFAULT_TZ
#define WALL_CLOCK_DEFAULT_TZ "GMT0BST,M3.5.0/1,M10.5.0"
#endif

static char s_tz_buf[64] = WALL_CLOCK_DEFAULT_TZ;
static bool s_sntp_started = false;
static bool s_synced_logged = false;
static bool s_sync_wait_done = false;
static unsigned long s_wifi_up_ms = 0;
static unsigned long s_last_sntp_retry_ms = 0;

static bool system_time_valid() {
    return time(nullptr) > 1700000000;
}

static bool wifi_has_ip() {
    return wifi_station_is_connected() && WiFi.localIP()[0] != 0;
}

static void apply_tz_env() {
    setenv("TZ", s_tz_buf, 1);
    tzset();
}

static void load_tz_nvs() {
    Preferences prefs;
    if (prefs.begin("time", true)) {
        String tz = prefs.getString("tz", WALL_CLOCK_DEFAULT_TZ);
        strncpy(s_tz_buf, tz.c_str(), sizeof(s_tz_buf) - 1);
        s_tz_buf[sizeof(s_tz_buf) - 1] = '\0';
        prefs.end();
    }
    if (s_tz_buf[0] == '\0') {
        strncpy(s_tz_buf, WALL_CLOCK_DEFAULT_TZ, sizeof(s_tz_buf) - 1);
        s_tz_buf[sizeof(s_tz_buf) - 1] = '\0';
    }
    apply_tz_env();
}

static void log_local_time_once() {
    if (s_synced_logged || !system_time_valid()) {
        return;
    }
    s_synced_logged = true;
    struct tm tm;
    time_t now = time(nullptr);
    localtime_r(&now, &tm);
    Serial.printf("[TIME] synced %04d-%02d-%02d %02d:%02d:%02d tz=%s\n", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, s_tz_buf);
}

static void start_sntp() {
    configTzTime(s_tz_buf, "pool.ntp.org", "time.google.com", nullptr);
    s_sntp_started = true;
    s_last_sntp_retry_ms = millis();
    Serial.printf("[TIME] SNTP started tz=%s\n", s_tz_buf);
}

void wall_clock_begin() {
    load_tz_nvs();
}

void wall_clock_poll() {
    const unsigned long now = millis();

    if (!wifi_has_ip()) {
        s_wifi_up_ms = 0;
        s_sync_wait_done = false;
        s_sntp_started = false;
        return;
    }

    if (s_wifi_up_ms == 0) {
        s_wifi_up_ms = now;
        s_sync_wait_done = false;
        s_synced_logged = false;
    }

    // Brief settle after DHCP before first SNTP (DNS).
    if (!s_sntp_started && now - s_wifi_up_ms >= 500) {
        start_sntp();
    }

    if (s_sntp_started && !s_sync_wait_done && now - s_wifi_up_ms >= 800) {
        s_sync_wait_done = true;
        struct tm tm;
        if (getLocalTime(&tm, 20000)) {
            log_local_time_once();
        } else {
            Serial.println("[TIME] SNTP wait timeout (will retry)");
        }
    }

    if (s_sntp_started && !system_time_valid() && now - s_last_sntp_retry_ms >= 45000) {
        Serial.println("[TIME] SNTP retry");
        start_sntp();
    }

    if (system_time_valid()) {
        log_local_time_once();
    }
}

bool wall_clock_is_synced() {
    if (!wifi_has_ip()) {
        return false;
    }
    return system_time_valid() ||
           (s_sntp_started && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED);
}

void wall_clock_now_hms(int& hour, int& minute, int& second) {
    struct tm tm;
    time_t t = time(nullptr);
    localtime_r(&t, &tm);
    hour = tm.tm_hour;
    minute = tm.tm_min;
    second = tm.tm_sec;
}

void wall_clock_now_ymd(int& year, int& month, int& day) {
    struct tm tm;
    time_t t = time(nullptr);
    localtime_r(&t, &tm);
    year = tm.tm_year + 1900;
    month = tm.tm_mon + 1;
    day = tm.tm_mday;
}

static void format_rfc3339_utc(time_t instant, char* buf, size_t len) {
    struct tm utc;
    gmtime_r(&instant, &utc);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &utc);
}

bool wall_clock_today_rfc3339_bounds(char* time_min, size_t min_len, char* time_max,
                                     size_t max_len) {
    if (!time_min || !time_max || min_len < 21 || max_len < 21) {
        return false;
    }
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    const time_t day_start = mktime(&tm);

    struct tm start_local;
    localtime_r(&day_start, &start_local);
    start_local.tm_mday += 1;
    const time_t day_end = mktime(&start_local);

    format_rfc3339_utc(day_start, time_min, min_len);
    format_rfc3339_utc(day_end, time_max, max_len);
    return time_min[0] != '\0' && time_max[0] != '\0';
}

bool wall_clock_set_tz_posix(const char* posix_tz) {
    if (!posix_tz || posix_tz[0] == '\0' || strlen(posix_tz) >= sizeof(s_tz_buf)) {
        return false;
    }
    strncpy(s_tz_buf, posix_tz, sizeof(s_tz_buf) - 1);
    s_tz_buf[sizeof(s_tz_buf) - 1] = '\0';
    Preferences prefs;
    if (!prefs.begin("time", false)) {
        return false;
    }
    prefs.putString("tz", s_tz_buf);
    prefs.end();
    if (wifi_has_ip()) {
        start_sntp();
    } else {
        apply_tz_env();
    }
    return true;
}

const char* wall_clock_tz_posix() {
    return s_tz_buf;
}

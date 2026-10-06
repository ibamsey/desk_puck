#include "tide/tide_feed.h"

#include "tide/tide_secrets.h"
#include "wifi/wifi_station.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const unsigned long kRefreshMs = 15UL * 60UL * 1000UL;
static const unsigned long kRetryAfterFailMs = 3UL * 60UL * 1000UL;
static const unsigned long kWifiSettleMs = 800;

enum class FetchStep : uint8_t {
    Idle,
    Observations,
    Predictions,
};

static TideFeedSnapshot s_snap;
static FetchStep s_step = FetchStep::Idle;
static unsigned long s_last_fetch_ms = 0;
static unsigned long s_last_fail_ms = 0;
static unsigned long s_wifi_up_ms = 0;
static bool s_force_refresh = true;

static WiFiClientSecure s_http_client;

static void clear_error() {
    s_snap.error[0] = '\0';
}

static void set_error(const char* msg) {
    strncpy(s_snap.error, msg ? msg : "error", sizeof(s_snap.error) - 1);
    s_snap.error[sizeof(s_snap.error) - 1] = '\0';
    s_last_fail_ms = millis();
    s_snap.loading = false;
    s_step = FetchStep::Idle;
}

static bool wifi_has_ip() {
    return wifi_station_is_connected() && WiFi.localIP()[0] != 0;
}

/** CCO date strings use station-local civil time (Exmouth), not UTC. */
static time_t cco_date_to_epoch(const char* s) {
    if (!s) {
        return 0;
    }
    int y = 0;
    int mo = 0;
    int d = 0;
    int h = 0;
    int mi = 0;
    int se = 0;
    if (sscanf(s, "%4d%2d%2d#%2d%2d%2d", &y, &mo, &d, &h, &mi, &se) != 6) {
        return 0;
    }
    struct tm tm = {};
    tm.tm_year = y - 1900;
    tm.tm_mon = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = se;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

static bool parse_height_before(const char* body, const char* ts_key_pos, const char* height_key,
                                float& out) {
    if (!body || !ts_key_pos || !height_key) {
        return false;
    }
    const size_t key_len = strlen(height_key);
    for (int back = 0; back <= 96; back++) {
        const char* q = ts_key_pos - back;
        if (q < body) {
            break;
        }
        if (strncmp(q, height_key, key_len) != 0) {
            continue;
        }
        out = strtof(q + key_len, nullptr);
        return true;
    }
    return false;
}

/** Staging buffer so a failed fetch cannot clobber the last good HW/LW list. */
struct HwLwSink {
    uint8_t count = 0;
    TideHwLwEvent items[TIDE_MAX_HWLW];
};

static void upsert_hwlw(HwLwSink& sink, time_t epoch, float height_m, bool is_high) {
    if (epoch <= 0) {
        return;
    }
    for (uint8_t i = 0; i < sink.count; i++) {
        TideHwLwEvent& e = sink.items[i];
        if (e.epoch != epoch || e.is_high != is_high) {
            continue;
        }
        if (is_high && height_m > e.height_m) {
            e.height_m = height_m;
        } else if (!is_high && height_m < e.height_m) {
            e.height_m = height_m;
        }
        return;
    }
    if (sink.count >= (uint8_t)TIDE_MAX_HWLW) {
        return;
    }
    TideHwLwEvent& e = sink.items[sink.count++];
    e.epoch = epoch;
    e.height_m = height_m;
    e.is_high = is_high;
}

static void scrape_hwlw_pair(const char* body, const char* limit, const char* height_key,
                             const char* ts_key, bool is_high, HwLwSink& sink) {
    const size_t ts_len = strlen(ts_key);
    const char* cur = body;
    while (sink.count < (uint8_t)TIDE_MAX_HWLW && (cur = strstr(cur, ts_key)) != nullptr) {
        const char* key_pos = cur;
        cur += ts_len;
        if (cur + 17 > limit) {
            return;
        }
        char date_buf[18];
        memcpy(date_buf, cur, 17);
        date_buf[17] = '\0';
        float height_m = 0.0f;
        if (parse_height_before(body, key_pos, height_key, height_m)) {
            upsert_hwlw(sink, cco_date_to_epoch(date_buf), height_m, is_high);
        }
    }
}

/** HW/LW times come from CCO's own prediction metadata, not from peak-finding. */
static void scrape_hwlw(const char* text, size_t len, HwLwSink& sink) {
    const char* limit = text + len;
    scrape_hwlw_pair(text, limit, "\"hw\":\"", "\"hw_timestamp\":\"", true, sink);
    scrape_hwlw_pair(text, limit, "\"lw\":\"", "\"lw_timestamp\":\"", false, sink);
    scrape_hwlw_pair(text, limit, "\"prev_hw\":\"", "\"prev_hw_timestamp\":\"", true, sink);
    scrape_hwlw_pair(text, limit, "\"prev_lw\":\"", "\"prev_lw_timestamp\":\"", false, sink);
}

static int compare_samples(const void* a, const void* b) {
    const TideSample* sa = static_cast<const TideSample*>(a);
    const TideSample* sb = static_cast<const TideSample*>(b);
    if (sa->epoch_utc < sb->epoch_utc) {
        return -1;
    }
    if (sa->epoch_utc > sb->epoch_utc) {
        return 1;
    }
    return 0;
}

static void sort_series(TideSeries& series) {
    if (series.count < 2) {
        return;
    }
    qsort(series.points, series.count, sizeof(TideSample), compare_samples);
}

/**
 * Parse whole date/value records out of a chunk and return how many bytes are safe to
 * discard. Anything left behind is a partial record that the next read completes.
 */
static size_t scan_samples_chunk(char* buf, size_t len, TideSeries& out) {
    size_t consumed = 0;
    char* cur = buf;
    char* const end = buf + len;

    while (out.count < (uint16_t)TIDE_MAX_SAMPLES) {
        char* key = strstr(cur, "\"date\":\"");
        if (!key) {
            // No further record starts here; keep just enough to rejoin a split key.
            if (len > 24) {
                consumed = len - 24;
            }
            break;
        }
        char* date = key + 8;
        if (date + 17 > end) {
            break;
        }
        char* val_key = strstr(date, "\"value\":");
        if (!val_key) {
            break;
        }
        char* val = val_key + 8;
        while (val < end && (*val == ' ' || *val == '\t' || *val == '"')) {
            val++;
        }
        char* num_end = val;
        while (num_end < end && (*num_end == '-' || *num_end == '+' || *num_end == '.' ||
                                 (*num_end >= '0' && *num_end <= '9'))) {
            num_end++;
        }
        if (num_end >= end) {
            break;
        }

        char date_buf[18];
        memcpy(date_buf, date, 17);
        date_buf[17] = '\0';
        const time_t epoch = cco_date_to_epoch(date_buf);
        if (epoch > 0) {
            TideSample& pt = out.points[out.count++];
            pt.epoch_utc = epoch;
            pt.height_m = strtof(val, nullptr);
        }
        cur = num_end;
        consumed = (size_t)(num_end - buf);
    }
    return consumed;
}

/**
 * Scan the response as it arrives. The 36h prediction payload is ~130 KB, far too big
 * to hold as a String alongside TLS buffers on an ESP32-C3.
 */
static bool stream_scan_series(HTTPClient& http, TideSeries& out, HwLwSink* hwlw_sink, bool required) {
    static const size_t kBufCap = 2048;
    static char buf[kBufCap + 1];

    out.count = 0;
    if (hwlw_sink) {
        hwlw_sink->count = 0;
    }

    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        if (required) {
            set_error("no stream");
        }
        return false;
    }

    size_t held = 0;
    size_t total = 0;
    int remaining = http.getSize();
    unsigned long last_data = millis();

    while (out.count < (uint16_t)TIDE_MAX_SAMPLES && (remaining > 0 || remaining == -1)) {
        size_t space = kBufCap - held;
        if (space == 0) {
            // Record longer than the buffer: drop the oldest half and carry on.
            memmove(buf, buf + kBufCap / 2, kBufCap / 2);
            held = kBufCap / 2;
            space = kBufCap / 2;
        }
        const int avail = stream->available();
        if (avail <= 0) {
            if (!http.connected() || millis() - last_data > 20000UL) {
                break;
            }
            delay(2);
            continue;
        }
        const size_t want = ((size_t)avail < space) ? (size_t)avail : space;
        const int got = stream->readBytes(buf + held, want);
        if (got <= 0) {
            delay(2);
            continue;
        }
        last_data = millis();
        held += (size_t)got;
        total += (size_t)got;
        if (remaining > 0) {
            remaining -= got;
        }
        buf[held] = '\0';

        if (hwlw_sink) {
            scrape_hwlw(buf, held, *hwlw_sink);
        }
        const size_t consumed = scan_samples_chunk(buf, held, out);
        if (consumed > 0) {
            held -= consumed;
            memmove(buf, buf + consumed, held);
            buf[held] = '\0';
        }
    }

    if (out.count == 0) {
        Serial.printf("[TIDE] scan 0 pts from %u bytes\n", (unsigned)total);
        if (required) {
            set_error("no tide data");
        }
        return false;
    }
    sort_series(out);
    Serial.printf("[TIDE] scan %u pts / %u bytes heap=%u\n", (unsigned)out.count, (unsigned)total,
                  (unsigned)ESP.getFreeHeap());
    return true;
}

/**
 * CCO request timestamps share the frame of the `date` strings they return, so build
 * them the same way we parse them: local civil time.
 */
static bool build_cco_timestamp(char* buf, size_t len, long offset_sec) {
    time_t now = time(nullptr);
    if (now <= 1700000000) {
        return false;
    }
    now += offset_sec;
    struct tm tm;
    localtime_r(&now, &tm);
    const int n = snprintf(buf, len, "%04d%02d%02d%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                           tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return n > 0 && (size_t)n < len;
}

static bool http_fetch_series(const char* observations_kind, const char* ts, unsigned duration_hours,
                              TideSeries& out, bool required, HwLwSink* hwlw_sink) {
    static bool tls_inited = false;
    if (!tls_inited) {
        s_http_client.setInsecure();
        tls_inited = true;
    }

    char url[320];
    snprintf(url, sizeof(url),
             "https://coastalmonitoring.org/observations/%s/%s.geojson?key=%s&sensor=%s&duration=%u",
             observations_kind, ts, TIDE_CCO_API_KEY, TIDE_SENSOR_ID, duration_hours);

    HTTPClient http;
    if (!http.begin(s_http_client, url)) {
        if (required) {
            set_error("http begin");
        }
        return false;
    }
    http.setTimeout(25000);
    http.addHeader("Referer", TIDE_CCO_REFERER);
    http.setReuse(false);

    const int code = http.GET();
    Serial.printf("[TIDE] GET %s dur=%uh code=%d len=%d heap=%u\n", observations_kind, duration_hours,
                  code, http.getSize(), (unsigned)ESP.getFreeHeap());

    if (code != HTTP_CODE_OK) {
        http.end();
        if (required) {
            set_error("http status");
        }
        return false;
    }

    const bool ok = stream_scan_series(http, out, hwlw_sink, required);
    http.end();
    return ok;
}

static bool fetch_observations() {
    char ts[16];
    if (!build_cco_timestamp(ts, sizeof(ts), 0)) {
        set_error("no time sync");
        Serial.println("[TIDE] waiting for SNTP");
        return false;
    }

    static TideSeries obs;
    obs.count = 0;
    if (!http_fetch_series("tides", ts, (unsigned)TIDE_OBS_DURATION_HOURS, obs, false, nullptr)) {
        Serial.println("[TIDE] obs skipped (pred may still load)");
        s_snap.observed.count = 0;
        return true;
    }
    s_snap.observed = obs;
    Serial.printf("[TIDE] obs %u points\n", (unsigned)s_snap.observed.count);
    return true;
}

static bool fetch_predictions() {
    // Forward window: a CCO call returns the `duration` hours ending at the timestamp.
    char ts[16];
    if (!build_cco_timestamp(ts, sizeof(ts), (long)TIDE_FUTURE_HOURS * 3600L)) {
        set_error("no time sync");
        return false;
    }

    static TideSeries pred;
    static HwLwSink sink;
    if (!http_fetch_series("tidepredictions", ts, (unsigned)TIDE_PRED_DURATION_HOURS, pred, true,
                           &sink)) {
        Serial.printf("[TIDE] pred fail: %s\n", s_snap.error);
        return false;
    }

    s_snap.predicted = pred;
    s_snap.hwlw_count = sink.count;
    memcpy(s_snap.hwlw, sink.items, sizeof(TideHwLwEvent) * sink.count);
    s_snap.valid = true;
    s_snap.loading = false;
    s_last_fetch_ms = millis();
    clear_error();
    Serial.printf("[TIDE] pred %u points hwlw=%u heap=%u\n", (unsigned)s_snap.predicted.count,
                  (unsigned)s_snap.hwlw_count, (unsigned)ESP.getFreeHeap());
    return true;
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

void tide_feed_begin() {
    s_force_refresh = true;
    s_step = FetchStep::Idle;
    s_snap = TideFeedSnapshot{};
}

void tide_feed_poll() {
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

    if (s_step == FetchStep::Idle) {
        if (s_last_fail_ms != 0 && now - s_last_fail_ms < kRetryAfterFailMs && !s_force_refresh) {
            return;
        }
        if (!should_refresh(now)) {
            return;
        }
        s_force_refresh = false;
        s_snap.loading = true;
        clear_error();
        s_step = FetchStep::Observations;
    }

    if (s_step == FetchStep::Observations) {
        if (fetch_observations()) {
            s_step = FetchStep::Predictions;
        }
        return;
    }

    if (s_step == FetchStep::Predictions) {
        if (fetch_predictions()) {
            s_step = FetchStep::Idle;
        }
    }
}

void tide_feed_snapshot(TideFeedSnapshot& out) {
    out = s_snap;
    if (out.valid && s_last_fetch_ms != 0) {
        out.fetched_age_ms = millis() - s_last_fetch_ms;
    }
}

bool tide_feed_is_ready() {
    return s_snap.valid;
}

void tide_feed_request_refresh() {
    s_force_refresh = true;
    s_step = FetchStep::Idle;
    s_last_fail_ms = 0;
    if (wifi_has_ip()) {
        s_snap.loading = true;
    }
}

bool tide_feed_is_busy() {
    return s_step != FetchStep::Idle || s_snap.loading;
}

const char* tide_feed_last_error() {
    return s_snap.error;
}

#ifndef TIDE_TIDE_FEED_H
#define TIDE_TIDE_FEED_H

#include <Arduino.h>

#ifndef TIDE_SENSOR_ID
#define TIDE_SENSOR_ID "117"
#endif

#ifndef TIDE_STATION_LABEL
#define TIDE_STATION_LABEL "Exmouth"
#endif

/** Display window: the past coil spans back, the future coil spans forward from now. */
#ifndef TIDE_PAST_HOURS
#define TIDE_PAST_HOURS 12
#endif

#ifndef TIDE_FUTURE_HOURS
#define TIDE_FUTURE_HOURS 24
#endif

/**
 * A CCO call returns the `duration` hours *ending* at the request timestamp, so the
 * forward window needs a timestamp of now + TIDE_FUTURE_HOURS.
 */
#define TIDE_PRED_DURATION_HOURS (TIDE_PAST_HOURS + TIDE_FUTURE_HOURS)
#define TIDE_OBS_DURATION_HOURS (TIDE_PAST_HOURS + 1)

/** 36h of 10-minute samples is 217; leave headroom for finer spacing. */
#define TIDE_MAX_SAMPLES 232
#define TIDE_MAX_HWLW 16

struct TideSample {
    time_t epoch_utc = 0;
    float height_m = 0.0f;
};

struct TideSeries {
    uint16_t count = 0;
    TideSample points[TIDE_MAX_SAMPLES];
};

/** High/low water from CCO prediction metadata (authoritative times). */
struct TideHwLwEvent {
    time_t epoch = 0;
    float height_m = 0.0f;
    bool is_high = false;
};

struct TideFeedSnapshot {
    TideSeries observed;
    TideSeries predicted;
    uint8_t hwlw_count = 0;
    TideHwLwEvent hwlw[TIDE_MAX_HWLW];
    bool valid = false;
    bool loading = false;
    unsigned long fetched_age_ms = 0;
    char error[48] = {};
};

void tide_feed_begin();

/** Rate-limited fetch when WiFi is up; one HTTP step per call. */
void tide_feed_poll();

void tide_feed_snapshot(TideFeedSnapshot& out);

bool tide_feed_is_ready();

void tide_feed_request_refresh();

/** True while a multi-step CCO fetch is in progress (wave feed should defer). */
bool tide_feed_is_busy();

const char* tide_feed_last_error();

#endif // TIDE_TIDE_FEED_H

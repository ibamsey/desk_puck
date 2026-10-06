#ifndef WAVES_WAVE_FEED_H
#define WAVES_WAVE_FEED_H

#include <Arduino.h>

#ifndef WAVE_SENSOR_ID
#define WAVE_SENSOR_ID "103"
#endif

#ifndef WAVE_STATION_LABEL
#define WAVE_STATION_LABEL "Dawlish"
#endif

struct WaveFeedSnapshot {
    float hs_m = 0.0f;
    float tp_s = 0.0f;
    float tz_s = 0.0f;
    time_t sample_epoch = 0;
    bool valid = false;
    bool loading = false;
    unsigned long fetched_age_ms = 0;
    char error[48] = {};
};

void wave_feed_begin();

void wave_feed_poll();

void wave_feed_snapshot(WaveFeedSnapshot& out);

bool wave_feed_is_ready();

void wave_feed_request_refresh();

const char* wave_feed_last_error();

#endif // WAVES_WAVE_FEED_H

#ifndef WEATHER_LOCAL_WEATHER_H
#define WEATHER_LOCAL_WEATHER_H

#include <Arduino.h>

/** Current conditions snapshot (Open-Meteo WMO weather_code). */
struct LocalWeatherNow {
    bool valid = false;
    int16_t temp_c = 0;
    uint8_t humidity_pct = 0;
    uint16_t wind_kmh = 0;
    int16_t weather_code = 0;
    /** Short label derived from weather_code (e.g. "Rain"). */
    char summary[24] = {};
    /** Milliseconds since last successful HTTP fetch (0 if never). */
    unsigned long fetched_age_ms = 0;
};

void local_weather_begin();

/** Call each loop; refreshes when WiFi is up (rate-limited). */
void local_weather_poll();

/** True when lat/lon are configured (NVS or compile-time defaults). */
bool local_weather_has_location();

bool local_weather_get_location(float& lat, float& lon);

/** Persist decimal degrees; triggers refresh on next poll. */
bool local_weather_set_location(float lat, float lon);

/** Cached or last-good snapshot; valid=false until first fetch or NVS load. */
void local_weather_now(LocalWeatherNow& out);

bool local_weather_is_ready();

/** Force HTTP refresh on next poll (no-op if offline). */
void local_weather_request_refresh();

/** Empty unless last fetch failed. */
const char* local_weather_last_error();

#endif // WEATHER_LOCAL_WEATHER_H

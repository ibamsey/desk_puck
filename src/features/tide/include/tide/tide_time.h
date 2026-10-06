#ifndef TIDE_TIDE_TIME_H
#define TIDE_TIDE_TIME_H

#include <stddef.h>
#include <time.h>

/**
 * CCO API timestamps in JSON are GMT. We store Unix instants and map to the dial
 * with localtime_r under the wall clock TZ (UK GMT/BST with DST).
 */
void tide_time_sync_tz();

/** Parse CCO date field to Unix time (instant on the timeline). */
time_t tide_time_from_cco_date(const char* cco_date);

/** Format a request timestamp for CCO URLs (local civil, DST-aware). */
bool tide_time_format_cco_request(time_t instant, char* buf, size_t len);

/** Seconds since local midnight for dial angles (0..86400). */
float tide_time_seconds_of_day(time_t instant);

#endif // TIDE_TIDE_TIME_H

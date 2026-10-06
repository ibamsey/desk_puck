#include "tide/tide_time.h"

#include "time/wall_clock.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void tide_time_sync_tz() {
    wall_clock_apply_tz();
}

/**
 * CCO GeoJSON uses GMT (UTC) in `date` and `*_timestamp` fields — see coastalmonitoring.org
 * API. Convert to Unix time, then use localtime_r for UK dial labels (BST/GMT via wall clock).
 */
time_t tide_time_from_cco_date(const char* cco_date) {
    if (!cco_date) {
        return 0;
    }
    int y = 0;
    int mo = 0;
    int d = 0;
    int h = 0;
    int mi = 0;
    int se = 0;
    if (sscanf(cco_date, "%4d%2d%2d#%2d%2d%2d", &y, &mo, &d, &h, &mi, &se) != 6) {
        return 0;
    }
    struct tm tm = {};
    tm.tm_year = y - 1900;
    tm.tm_mon = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = se;
    tm.tm_isdst = 0;

    char saved_tz[64] = {};
    const char* cur = getenv("TZ");
    if (cur) {
        strncpy(saved_tz, cur, sizeof(saved_tz) - 1);
    }
    setenv("TZ", "GMT0", 1);
    tzset();
    const time_t t = mktime(&tm);
    if (saved_tz[0] != '\0') {
        setenv("TZ", saved_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    wall_clock_apply_tz();
    return t;
}

bool tide_time_format_cco_request(time_t instant, char* buf, size_t len) {
    if (!buf || len < 16) {
        return false;
    }
    struct tm tm;
    gmtime_r(&instant, &tm);
    const int n = snprintf(buf, len, "%04d%02d%02d%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                           tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return n > 0 && (size_t)n < len;
}

float tide_time_seconds_of_day(time_t instant) {
    tide_time_sync_tz();
    struct tm tm;
    localtime_r(&instant, &tm);
    return (float)(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
}

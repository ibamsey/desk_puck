#include "clock/wall_time.h"

#include "config.h"
#include "time/wall_clock.h"

#include <time.h>

static time_t s_epoch_at_boot = 0;
static uint32_t s_millis_at_boot = 0;
static int s_offset_sec = 0;

static time_t compile_time_epoch() {
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* date = __DATE__;
    const char* time_str = __TIME__;

    char month_str[4] = {date[0], date[1], date[2], '\0'};
    int day = (date[4] == ' ' ? 0 : 10) + (date[5] - '0');
    int year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 +
               (date[10] - '0');

    int month = 0;
    for (int i = 0; i < 12; ++i) {
        if (strncmp(month_str, month_names + i * 3, 3) == 0) {
            month = i;
            break;
        }
    }

    int hour = (time_str[0] - '0') * 10 + (time_str[1] - '0');
    int minute = (time_str[3] - '0') * 10 + (time_str[4] - '0');
    int second = (time_str[6] - '0') * 10 + (time_str[7] - '0');

    struct tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = -1;

    return mktime(&tm);
}

static bool system_epoch_valid() {
    return time(nullptr) > 1700000000;
}

static void fill_from_tm(ClockWallTime& out, const struct tm& tm) {
    out.year = tm.tm_year + 1900;
    out.month = tm.tm_mon + 1;
    out.day = tm.tm_mday;
    out.weekday = tm.tm_wday;
    out.hour = tm.tm_hour;
    out.minute = tm.tm_min;
    out.second = tm.tm_sec;
}

static void wall_time_now_tm(struct tm& tm) {
    if (system_epoch_valid() || wall_clock_is_synced()) {
        time_t t = time(nullptr);
        localtime_r(&t, &tm);
        return;
    }

    const time_t elapsed = (millis() - s_millis_at_boot) / 1000;
    time_t t = s_epoch_at_boot + elapsed + s_offset_sec;
    localtime_r(&t, &tm);
}

void wall_time_begin() {
    wall_clock_begin();
    s_millis_at_boot = millis();
    s_epoch_at_boot = compile_time_epoch();
    s_offset_sec = CLOCK_UTC_OFFSET_SEC;
}

void wall_time_now(ClockWallTime& out) {
    struct tm tm{};
    wall_time_now_tm(tm);
    fill_from_tm(out, tm);
}

void wall_time_now(int& hour, int& minute, int& second) {
    ClockWallTime wall;
    wall_time_now(wall);
    hour = wall.hour;
    minute = wall.minute;
    second = wall.second;
}

void wall_time_add_offset_seconds(int delta) {
    s_offset_sec += delta;
}

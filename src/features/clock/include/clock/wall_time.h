#ifndef CLOCK_WALL_TIME_H
#define CLOCK_WALL_TIME_H

#include <Arduino.h>

/** Calendar wall clock (local TZ when SNTP has synced). */
struct ClockWallTime {
    int year = 0;
    int month = 0;   /** 1–12 */
    int day = 0;     /** 1–31 */
    int weekday = 0; /** 0 = Sunday … 6 = Saturday (same as struct tm) */
    int hour = 0;
    int minute = 0;
    int second = 0;
};

/** Wall clock from boot: compile time + millis(), with optional offset. */
void wall_time_begin();
void wall_time_now(ClockWallTime& out);
void wall_time_now(int& hour, int& minute, int& second);
void wall_time_add_offset_seconds(int delta);

#endif // CLOCK_WALL_TIME_H

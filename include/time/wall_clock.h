#ifndef TIME_WALL_CLOCK_H
#define TIME_WALL_CLOCK_H

#include <Arduino.h>
#include <time.h>

void wall_clock_begin();

/** Re-apply POSIX TZ to libc (GMT/BST + DST). Safe before localtime/mktime. */
void wall_clock_apply_tz();

/** Call each loop; starts SNTP when WiFi up. */
void wall_clock_poll();

bool wall_clock_is_synced();

void wall_clock_now_hms(int& hour, int& minute, int& second);

void wall_clock_now_ymd(int& year, int& month, int& day);

/** Local midnight boundaries for Calendar timeMin/timeMax (RFC3339 into buf). */
bool wall_clock_today_rfc3339_bounds(char* time_min, size_t min_len, char* time_max, size_t max_len);

/** IANA name stored as POSIX TZ string in NVS; apply with tz set. */
bool wall_clock_set_tz_posix(const char* posix_tz);

const char* wall_clock_tz_posix();

#endif // TIME_WALL_CLOCK_H

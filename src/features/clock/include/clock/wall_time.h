#ifndef CLOCK_WALL_TIME_H
#define CLOCK_WALL_TIME_H

#include <Arduino.h>

/** Wall clock from boot: compile time + millis(), with optional offset. */
void wall_time_begin();
void wall_time_now(int& hour, int& minute, int& second);
void wall_time_add_offset_seconds(int delta);

#endif // CLOCK_WALL_TIME_H

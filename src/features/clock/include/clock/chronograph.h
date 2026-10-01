#ifndef CLOCK_CHRONOGRAPH_H
#define CLOCK_CHRONOGRAPH_H

#include <Arduino.h>

/** Stopwatch: tap start → tap stop → double-tap reset (when face is chronograph). */
class Chronograph {
public:
    void reset();
    void start(unsigned long now_ms);
    void stop(unsigned long now_ms);

    bool is_running() const { return _running; }
    unsigned long elapsed_ms(unsigned long now_ms) const;

private:
    bool _running = false;
    unsigned long _accumulated_ms = 0;
    unsigned long _run_start_ms = 0;
};

/** Elapsed → hand angle (0–360°, one lap per 60 s). */
float chronograph_second_angle(unsigned long elapsed_ms);

/** Elapsed → minute counter hand (one lap per 60 min). */
float chronograph_minute_angle(unsigned long elapsed_ms);

#endif // CLOCK_CHRONOGRAPH_H

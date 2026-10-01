#include "clock/chronograph.h"

void Chronograph::reset() {
    _running = false;
    _accumulated_ms = 0;
    _run_start_ms = 0;
}

void Chronograph::start(unsigned long now_ms) {
    if (_running) {
        return;
    }
    _run_start_ms = now_ms;
    _running = true;
}

void Chronograph::stop(unsigned long now_ms) {
    if (!_running) {
        return;
    }
    _accumulated_ms += now_ms - _run_start_ms;
    _running = false;
}

unsigned long Chronograph::elapsed_ms(unsigned long now_ms) const {
    if (_running) {
        return _accumulated_ms + (now_ms - _run_start_ms);
    }
    return _accumulated_ms;
}

float chronograph_second_angle(unsigned long elapsed_ms) {
    const unsigned long lap = elapsed_ms % 60000UL;
    return (float)lap * 360.0f / 60000.0f;
}

float chronograph_minute_angle(unsigned long elapsed_ms) {
    const unsigned long lap = elapsed_ms % 3600000UL;
    return (float)lap * 360.0f / 3600000.0f;
}

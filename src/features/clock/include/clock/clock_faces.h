#ifndef CLOCK_CLOCK_FACES_H
#define CLOCK_CLOCK_FACES_H

#include "LGFX_config.h"

struct AnalogClockState {
    float hour_angle;
    float minute_angle;
    float second_angle;
    unsigned long chrono_elapsed_ms = 0;
    bool draw_chrono = false;
};

struct ClockFace {
    const char* id;
    const char* name;
    void (*draw_background)(LGFX_Device& lcd);
    void (*draw_static)(LGFX_Device& lcd);
    void (*draw_hands)(LGFX_Device& lcd, const AnalogClockState& state, bool erase);
    uint16_t erase_color;
};

enum class ClockFaceKind : uint8_t { Procedural, Asset };

struct ClockFaceEntry {
    const char* id;
    const char* name;
    ClockFaceKind kind;
    union {
        const ClockFace* procedural;
        uint16_t asset_index;
    };
};

int clock_face_count();
const ClockFaceEntry* clock_face_entry_at(int index);

#endif // CLOCK_CLOCK_FACES_H

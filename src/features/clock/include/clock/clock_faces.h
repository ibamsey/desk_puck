#ifndef CLOCK_CLOCK_FACES_H
#define CLOCK_CLOCK_FACES_H

#include "LGFX_config.h"

struct AnalogClockState {
    int hour = 0;
    int minute = 0;
    float hour_angle;
    float minute_angle;
    float second_angle;
    unsigned long chrono_elapsed_ms = 0;
    bool draw_chrono = false;
};

enum class ClockHandMask : uint8_t {
    None = 0,
    Hour = 1 << 0,
    Minute = 1 << 1,
    Second = 1 << 2,
    Hub = 1 << 3,
    ChronoSubdials = 1 << 4,
    Dial = 1 << 5,
    WallHands = Hour | Minute | Second | Hub,
    All = 0xFF,
};

inline ClockHandMask operator|(ClockHandMask a, ClockHandMask b) {
    return static_cast<ClockHandMask>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline ClockHandMask& operator|=(ClockHandMask& a, ClockHandMask b) {
    a = a | b;
    return a;
}
inline bool any_hand(ClockHandMask m) {
    return (static_cast<uint8_t>(m) & static_cast<uint8_t>(ClockHandMask::WallHands)) != 0;
}

struct ClockFace {
    const char* id;
    const char* name;
    void (*draw_background)(LGFX_Device& lcd);
    void (*draw_static)(LGFX_Device& lcd);
    void (*draw_hands)(LGFX_Device& lcd, const AnalogClockState& state, bool erase, ClockHandMask mask);
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

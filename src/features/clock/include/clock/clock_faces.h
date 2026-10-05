#ifndef CLOCK_CLOCK_FACES_H
#define CLOCK_CLOCK_FACES_H

#include "LGFX_config.h"

#include "clock/wall_time.h"

struct AnalogClockState {
    ClockWallTime wall;
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
    void (*draw_background)(lgfx::LovyanGFX& lcd);
    /** Tick marks and labels baked into the static layer (no moving hands). */
    void (*draw_static)(lgfx::LovyanGFX& lcd, const ClockWallTime& wall);
    /** Optional digital readout baked into the static layer (behind hands); compositor draws second last. */
    void (*draw_digital_overlay)(lgfx::LovyanGFX& lcd, const ClockWallTime& wall);
    /**
     * Draw hands with the dial centre at (cx, cy). The compositor passes an offset centre so
     * hands can be rendered into a small off-screen patch; faces must not hard-code the centre.
     */
    void (*draw_hands)(lgfx::LovyanGFX& lcd, const AnalogClockState& state, bool erase, ClockHandMask mask,
                       int cx, int cy);
    /** Second hand reach from the centre (tail, tip), used to size the compositor dirty rect. */
    float second_tail_len;
    float second_tip_len;
    uint16_t erase_color;
    /** Redraw draw_static when the second changes (live digital seconds). */
    bool refresh_static_every_second;
    /** Bake dial + hour/minute into a RAM static layer; smooth second via dirty-rect restore. */
    bool use_static_compositor;
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

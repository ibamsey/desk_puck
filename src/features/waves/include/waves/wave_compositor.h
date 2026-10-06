#ifndef WAVES_WAVE_COMPOSITOR_H
#define WAVES_WAVE_COMPOSITOR_H

#include "LGFX_config.h"

/** One full-frame RGB565 buffer (~115 KB) — fits ESP32-C3 heap alongside WiFi. */
class WaveCompositor {
public:
    void reset();
    /** Try to allocate the off-screen frame (safe to call from onEnter). */
    bool ensure_buffer();
    void present(lgfx::LGFX_Device& display, float hs_m, float tp_s, unsigned long anim_origin_ms,
                 unsigned long now_ms);

private:
    bool ensure_frame_buffer();
    void present_full_frame(lgfx::LGFX_Device& display, float hs_m, float tp_s,
                            unsigned long anim_origin_ms, unsigned long now_ms);
    void present_direct(lgfx::LGFX_Device& display, float hs_m, float tp_s, unsigned long anim_origin_ms,
                        unsigned long now_ms);

    uint16_t* _frame = nullptr;
};

#endif // WAVES_WAVE_COMPOSITOR_H

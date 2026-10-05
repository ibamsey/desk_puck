#ifndef CLOCK_PROCEDURAL_COMPOSITOR_H
#define CLOCK_PROCEDURAL_COMPOSITOR_H

#include "clock/clock_faces.h"
#include "LGFX_config.h"

struct ProceduralCompositorRect {
    int x;
    int y;
    int w;
    int h;
    bool empty() const { return w <= 0 || h <= 0; }
};

/**
 * Static layer = dial + digital (behind hands) + hour/minute/hub, rebuilt every frame.
 * Readout bands blit from static except where the second-hand patch will draw (hand last).
 */
class ProceduralCompositor {
public:
    void reset();
    void present(lgfx::LGFX_Device& gfx, const ClockFace* face, const AnalogClockState& state,
                 bool force_full);

private:
    using Rect = ProceduralCompositorRect;

    bool ensure_static();
    bool ensure_patch(size_t pixel_count);
    bool rebuild_static(const ClockFace* face, const AnalogClockState& state);
    Rect second_hand_rect(const ClockFace* face, float angle_deg) const;
    static Rect clip(Rect r);
    static Rect merge(Rect a, Rect b);
    static bool intersects(const Rect& a, const Rect& b);
    static Rect intersection(Rect a, Rect b);
    void copy_static_rect_to_patch(Rect patch_origin, Rect src_on_screen);
    bool push_band_patch(lgfx::LGFX_Device& gfx, Rect band);
    bool push_hand_composite_patch(lgfx::LGFX_Device& gfx, const ClockFace* face,
                                   const AnalogClockState& state, Rect hand);
    void push_full(lgfx::LGFX_Device& gfx, const ClockFace* face, const AnalogClockState& state);
    void remember_shown(const AnalogClockState& state);

    uint16_t* _static = nullptr;
    uint16_t* _patch = nullptr;
    size_t _patch_pixels = 0;
    bool _static_valid = false;
    bool _has_shown_second = false;
    float _shown_second_angle = 0.0f;
};

#endif // CLOCK_PROCEDURAL_COMPOSITOR_H

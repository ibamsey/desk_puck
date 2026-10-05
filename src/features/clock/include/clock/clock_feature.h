#ifndef CLOCK_CLOCK_FEATURE_H
#define CLOCK_CLOCK_FEATURE_H

#include "clock/asset_face.h"
#include "clock/chronograph.h"
#include "clock/clock_faces.h"
#include "clock/procedural_compositor.h"
#include "feature.h"

class ClockFeature : public Feature {
public:
    const char* name() const override { return "Clock"; }

    void onEnter() override;
    void onExit() override;
    void onTick(unsigned long now_ms) override;
    void onDraw(lgfx::LGFX_Device& gfx) override;
    bool onInput(InputEvent event) override;

private:
    AnalogClockState build_clock_state(int hour, int minute, int second, unsigned long now_ms);
    ClockHandMask procedural_mask(int hour, int minute, int second, bool need_full) const;
    bool active_face_has_smooth_motion() const;

    void request_full();
    void advance_face(int delta);
    void sync_asset_for_face(int face_index);
    bool active_face_is_chronograph() const;
    void reset_chronograph();

    int _face_index = 0;
    int _last_hour = -1;
    int _last_minute = -1;
    int _last_second = -1;
    int _last_static_minute = -1;
    int _last_static_day = -1;
    int _last_static_second = -1;
    AnalogClockState _last_drawn{};
    bool _static_drawn = false;
    bool _force_full = true;
    bool _face_composed = false;
    int _frac_anchor_second = -1;
    unsigned long _frac_anchor_ms = 0;
    unsigned long _second_smooth_last_draw_ms = 0;
    unsigned long _face_gesture_cooldown_until_ms = 0;
    AssetFaceRuntime _asset;
    ProceduralCompositor _procedural;
    int _loaded_face_index = -1;
    Chronograph _chrono;
    unsigned long _chrono_last_tap_ms = 0;
    unsigned long _chrono_pending_toggle_ms = 0;
    static constexpr unsigned long kChronoDoubleTapWindowMs = 450;
    static constexpr unsigned long kChronoSingleTapDelayMs = 280;
};

#endif // CLOCK_CLOCK_FEATURE_H

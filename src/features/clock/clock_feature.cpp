#include "clock/clock_feature.h"

#include "clock/clock_faces.h"
#include "clock/generated/watch_face_manifest.h"
#include "clock/wall_time.h"
#include "config.h"

#include <time.h>

namespace {

static AnalogClockState angles_from_hms(int hour, int minute, int second, float second_frac) {
    AnalogClockState s;
    s.hour = hour;
    s.minute = minute;
    s.minute_angle = (float)minute * 6.0f + (float)second * 0.1f;
    s.hour_angle = (float)(hour % 12) * 30.0f + (float)minute * 0.5f;
    const float sec = (float)second + second_frac;
    s.second_angle = sec * 6.0f;
    return s;
}

static bool mask_has(ClockHandMask mask, ClockHandMask bit) {
    return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(bit)) != 0;
}

} // namespace

AnalogClockState ClockFeature::build_clock_state(int hour, int minute, int second,
                                                 unsigned long now_ms) {
    if (second != _frac_anchor_second) {
        _frac_anchor_second = second;
        _frac_anchor_ms = now_ms;
    }
    float frac = 0.0f;
    if (_frac_anchor_ms != 0) {
        frac = (float)(now_ms - _frac_anchor_ms) / 1000.0f;
        if (frac < 0.0f) {
            frac = 0.0f;
        } else if (frac > 1.0f) {
            frac = 1.0f;
        }
    }
    return angles_from_hms(hour, minute, second, frac);
}

ClockHandMask ClockFeature::procedural_mask(int hour, int minute, int second, bool need_full) const {
    if (need_full) {
        return ClockHandMask::WallHands;
    }
    if (hour != _last_hour) {
        return ClockHandMask::WallHands;
    }
    if (minute != _last_minute) {
        return ClockHandMask::Minute | ClockHandMask::Second | ClockHandMask::Hub;
    }
    if (second != _last_second) {
        return ClockHandMask::Second | ClockHandMask::Hub;
    }
    return ClockHandMask::Second;
}

bool ClockFeature::active_face_has_smooth_motion() const {
    const ClockFaceEntry* entry = clock_face_entry_at(_face_index);
    if (!entry) {
        return false;
    }
    if (entry->kind == ClockFaceKind::Procedural) {
        return true;
    }
    const AssetFaceMeta* meta = asset_face_meta(entry->asset_index);
    if (!meta) {
        return false;
    }
    if (meta->has_second()) {
        return true;
    }
    return meta->is_chronograph() && _chrono.is_running();
}

bool ClockFeature::active_face_is_chronograph() const {
    const ClockFaceEntry* entry = clock_face_entry_at(_face_index);
    if (!entry || entry->kind != ClockFaceKind::Asset) {
        return false;
    }
    const AssetFaceMeta* meta = asset_face_meta(entry->asset_index);
    return meta && meta->is_chronograph();
}

void ClockFeature::request_full() {
    _force_full = true;
    _face_composed = false;
    _static_drawn = false;
    _last_hour = _last_minute = _last_second = -1;
    setDirty();
}

void ClockFeature::reset_chronograph() {
    _chrono.reset();
    _chrono_last_tap_ms = 0;
    _chrono_pending_toggle_ms = 0;
}

void ClockFeature::sync_asset_for_face(int face_index) {
    const ClockFaceEntry* entry = clock_face_entry_at(face_index);
    if (!entry) {
        _asset.unload();
        _loaded_face_index = -1;
        return;
    }

    if (entry->kind != ClockFaceKind::Asset) {
        if (_loaded_face_index >= 0) {
            _asset.unload();
            _loaded_face_index = -1;
        }
        return;
    }

    if (_loaded_face_index == face_index && _asset.is_loaded()) {
        return;
    }

    const AssetFaceMeta* meta = asset_face_meta(entry->asset_index);
    if (!meta || !_asset.load((int)entry->asset_index, meta)) {
        Serial.printf("[CLOCK] asset load failed: %s\n", entry->id);
        _loaded_face_index = -1;
        return;
    }
    _loaded_face_index = face_index;
}

void ClockFeature::onEnter() {
    static bool wall_time_started = false;
    if (!wall_time_started) {
        wall_time_begin();
        wall_time_started = true;
    }
    _frac_anchor_second = -1;
    _frac_anchor_ms = 0;
    _second_smooth_last_draw_ms = 0;
    _face_gesture_cooldown_until_ms = millis() + GESTURE_ECHO_COOLDOWN_MS;
    reset_chronograph();
    sync_asset_for_face(_face_index);
    request_full();
}

void ClockFeature::onExit() {
    reset_chronograph();
    _asset.unload();
    _loaded_face_index = -1;
    _face_composed = false;
}

void ClockFeature::advance_face(int delta) {
    const int count = clock_face_count();
    if (count <= 1) {
        return;
    }
    _face_index = (_face_index + delta + count) % count;
    _frac_anchor_second = -1;
    _frac_anchor_ms = 0;
    _second_smooth_last_draw_ms = 0;
    reset_chronograph();
    _face_gesture_cooldown_until_ms = millis() + GESTURE_ECHO_COOLDOWN_MS;
    sync_asset_for_face(_face_index);
    request_full();

#if DEBUG_DISPLAY
    const ClockFaceEntry* entry = clock_face_entry_at(_face_index);
    if (entry) {
        Serial.printf("[CLOCK] face=%s (%s)\n", entry->id, entry->name);
    }
#endif
}

bool ClockFeature::onInput(InputEvent event) {
    if (active_face_is_chronograph() &&
        (event == InputEvent::DoubleTap || event == InputEvent::Tap)) {
        const unsigned long now = millis();

        if (event == InputEvent::DoubleTap) {
            reset_chronograph();
#if DEBUG_TOUCH
            Serial.println("[CLOCK] chrono reset (double)");
#endif
            request_full();
            return true;
        }

        if (_chrono_last_tap_ms != 0 && now - _chrono_last_tap_ms <= kChronoDoubleTapWindowMs) {
            reset_chronograph();
#if DEBUG_TOUCH
            Serial.println("[CLOCK] chrono reset (two taps)");
#endif
            request_full();
            return true;
        }

        _chrono_last_tap_ms = now;
        _chrono_pending_toggle_ms = now;
#if DEBUG_TOUCH
        Serial.println("[CLOCK] chrono tap (pending start/stop)");
#endif
        return true;
    }

    if (event != InputEvent::SwipeUp && event != InputEvent::SwipeDown) {
        return false;
    }

    if (millis() < _face_gesture_cooldown_until_ms) {
#if DEBUG_TOUCH
        Serial.println("[CLOCK] face swipe ignored (cooldown)");
#endif
        return true;
    }

    if (event == InputEvent::SwipeUp) {
        advance_face(1);
    } else {
        advance_face(-1);
    }
    return true;
}

void ClockFeature::onTick(unsigned long now_ms) {
    if (_chrono_pending_toggle_ms != 0 &&
        now_ms - _chrono_pending_toggle_ms >= kChronoSingleTapDelayMs) {
        _chrono_pending_toggle_ms = 0;
        _chrono_last_tap_ms = 0;
        if (_chrono.is_running()) {
            _chrono.stop(now_ms);
#if DEBUG_TOUCH
            Serial.println("[CLOCK] chrono stop");
#endif
        } else {
            _chrono.start(now_ms);
#if DEBUG_TOUCH
            Serial.println("[CLOCK] chrono start");
#endif
        }
        request_full();
    }

    static bool had_network_epoch = false;
    static time_t last_network_epoch = 0;

    const time_t epoch = time(nullptr);
    const bool network_epoch = epoch > 1700000000;

    if (network_epoch && !had_network_epoch) {
        request_full();
    }
    if (network_epoch && last_network_epoch > 0 &&
        (epoch > last_network_epoch + 2 || epoch + 2 < last_network_epoch)) {
        request_full();
    }
    had_network_epoch = network_epoch;
    if (network_epoch) {
        last_network_epoch = epoch;
    } else {
        last_network_epoch = 0;
    }

    int h, m, s;
    wall_time_now(h, m, s);

    bool need_draw = (h != _last_hour) || (m != _last_minute) || (s != _last_second);
    if (active_face_has_smooth_motion() &&
        now_ms - _second_smooth_last_draw_ms >= (unsigned long)CLOCK_SECOND_SMOOTH_INTERVAL_MS) {
        _second_smooth_last_draw_ms = now_ms;
        need_draw = true;
    }

    if (need_draw) {
        setDirty();
    }
}

void ClockFeature::onDraw(lgfx::LGFX_Device& gfx) {
    const ClockFaceEntry* entry = clock_face_entry_at(_face_index);
    if (!entry) {
        return;
    }

    const unsigned long now_ms = millis();
    int h, m, s;
    wall_time_now(h, m, s);
    AnalogClockState angles = build_clock_state(h, m, s, now_ms);
    if (active_face_is_chronograph()) {
        angles.draw_chrono = true;
        angles.chrono_elapsed_ms = _chrono.elapsed_ms(now_ms);
    }

    const bool need_full = _force_full || !_face_composed;

    if (entry->kind == ClockFaceKind::Asset) {
        if (!_asset.is_loaded()) {
            sync_asset_for_face(_face_index);
        }
        _asset.present(gfx, angles, need_full);
    } else {
        const ClockFace* face = entry->procedural;
        if (!face) {
            return;
        }
        if (need_full || !_static_drawn) {
            face->draw_background(gfx);
            face->draw_static(gfx);
            _static_drawn = true;
            face->draw_hands(gfx, angles, false, ClockHandMask::WallHands);
        } else {
            const ClockHandMask mask = procedural_mask(h, m, s, false);
            const bool second_only =
                mask_has(mask, ClockHandMask::Second) && !mask_has(mask, ClockHandMask::Hour) &&
                !mask_has(mask, ClockHandMask::Minute);

            if (second_only) {
                face->draw_hands(gfx, _last_drawn, true, ClockHandMask::Second);
                face->draw_hands(gfx, angles, false, ClockHandMask::Second);
                if (mask_has(mask, ClockHandMask::Hub)) {
                    face->draw_hands(gfx, angles, false, ClockHandMask::Hub);
                }
            } else {
                face->draw_hands(gfx, _last_drawn, true, mask);
                face->draw_hands(gfx, angles, false, mask);
                if (mask_has(mask, ClockHandMask::Minute) && !mask_has(mask, ClockHandMask::Hour)) {
                    face->draw_hands(gfx, angles, false, ClockHandMask::Hour);
                }
            }
        }
    }

    _last_drawn = angles;
    _face_composed = true;
    _force_full = false;
    _last_hour = h;
    _last_minute = m;
    _last_second = s;
}

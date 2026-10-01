#include "clock/clock_feature.h"

#include "clock/clock_faces.h"
#include "clock/generated/watch_face_manifest.h"
#include "clock/wall_time.h"
#include "config.h"

#include <time.h>

static AnalogClockState angles_from_time(int hour, int minute, int second) {
    AnalogClockState s;
    s.minute_angle = (float)minute * 6.0f + (float)second * 0.1f;
    s.hour_angle = (float)(hour % 12) * 30.0f + (float)minute * 0.5f;
    s.second_angle = (float)second * 6.0f;
    return s;
}

bool ClockFeature::active_face_is_chronograph() const {
    const ClockFaceEntry* entry = clock_face_entry_at(_face_index);
    if (!entry || entry->kind != ClockFaceKind::Asset) {
        return false;
    }
    const AssetFaceMeta* meta = asset_face_meta(entry->asset_index);
    return meta && meta->is_chronograph();
}

void ClockFeature::reset_chronograph() {
    _chrono.reset();
    _chrono_last_tap_ms = 0;
    _chrono_pending_toggle_ms = 0;
    _chrono_last_draw_ms = 0;
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
    _static_drawn = false;
    _redraw_mode = RedrawMode::Full;
    _last_hour = _last_minute = _last_second = -1;
    _face_gesture_cooldown_until_ms = millis() + GESTURE_ECHO_COOLDOWN_MS;
    reset_chronograph();
    sync_asset_for_face(_face_index);
    setDirty();
}

void ClockFeature::onExit() {
    reset_chronograph();
    _asset.unload();
    _loaded_face_index = -1;
}

void ClockFeature::advance_face(int delta) {
    const int count = clock_face_count();
    if (count <= 1) {
        return;
    }
    _face_index = (_face_index + delta + count) % count;
    _static_drawn = false;
    _redraw_mode = RedrawMode::Full;
    _last_hour = _last_minute = _last_second = -1;
    reset_chronograph();
    _face_gesture_cooldown_until_ms = millis() + GESTURE_ECHO_COOLDOWN_MS;
    sync_asset_for_face(_face_index);
    setDirty();

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
            setDirty();
            return true;
        }

        if (_chrono_last_tap_ms != 0 && now - _chrono_last_tap_ms <= kChronoDoubleTapWindowMs) {
            reset_chronograph();
#if DEBUG_TOUCH
            Serial.println("[CLOCK] chrono reset (two taps)");
#endif
            setDirty();
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
        _chrono_last_draw_ms = 0;
        setDirty();
    }

    static bool had_network_epoch = false;
    static time_t last_network_epoch = 0;

    const time_t epoch = time(nullptr);
    const bool network_epoch = epoch > 1700000000;

    if (network_epoch && !had_network_epoch) {
        _last_hour = _last_minute = _last_second = -1;
        _redraw_mode = RedrawMode::Full;
        setDirty();
    }
    if (network_epoch && last_network_epoch > 0 &&
        (epoch > last_network_epoch + 2 || epoch + 2 < last_network_epoch)) {
        _last_hour = _last_minute = _last_second = -1;
        _redraw_mode = RedrawMode::Full;
        setDirty();
    }
    had_network_epoch = network_epoch;
    if (network_epoch) {
        last_network_epoch = epoch;
    } else {
        last_network_epoch = 0;
    }

    int h, m, s;
    wall_time_now(h, m, s);
    const bool wall_changed = h != _last_hour || m != _last_minute || s != _last_second;

    const ClockFaceEntry* entry = clock_face_entry_at(_face_index);
    const bool is_asset = entry && entry->kind == ClockFaceKind::Asset;

    const bool chrono_face = active_face_is_chronograph();
    const bool chrono_tick = chrono_face && (now_ms - _chrono_last_draw_ms >= 50) &&
                             (_chrono.is_running() || s != _last_second);

    if (!wall_changed && !chrono_tick) {
        return;
    }
    if (chrono_tick) {
        _chrono_last_draw_ms = now_ms;
    }
    _redraw_mode = is_asset ? RedrawMode::Full : RedrawMode::HandsOnly;
    setDirty();
}

void ClockFeature::onDraw(lgfx::LGFX_Device& gfx) {
    const ClockFaceEntry* entry = clock_face_entry_at(_face_index);
    if (!entry) {
        return;
    }

    int h, m, s;
    wall_time_now(h, m, s);
    AnalogClockState angles = angles_from_time(h, m, s);
    if (active_face_is_chronograph()) {
        angles.draw_chrono = true;
        angles.chrono_elapsed_ms = _chrono.elapsed_ms(millis());
    }

    if (entry->kind == ClockFaceKind::Asset) {
        if (!_asset.is_loaded()) {
            sync_asset_for_face(_face_index);
        }
        _asset.draw(gfx, angles);
    } else {
        const ClockFace* face = entry->procedural;
        if (!face) {
            return;
        }
        if (_redraw_mode == RedrawMode::Full || !_static_drawn) {
            face->draw_background(gfx);
            face->draw_static(gfx);
            _static_drawn = true;
            _last_drawn = angles;
            face->draw_hands(gfx, _last_drawn, false);
        } else {
            face->draw_hands(gfx, _last_drawn, true);
            _last_drawn = angles;
            face->draw_hands(gfx, _last_drawn, false);
        }
    }

    _last_hour = h;
    _last_minute = m;
    _last_second = s;
    _redraw_mode = RedrawMode::HandsOnly;
}

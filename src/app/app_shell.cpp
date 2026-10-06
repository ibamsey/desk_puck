#include "app_shell.h"

#include "clock/clock_feature.h"
#include "config.h"
#include "diary/diary_feature.h"
#include "cube/cube_feature.h"
#include "tide/tide_feature.h"
#include "waves/wave_feature.h"

static ClockFeature s_clock;
static DiaryFeature s_diary;
static CubeFeature s_cube;
static TideFeature s_tide;
static WaveFeature s_waves;

static Feature* s_registry[] = {
    &s_clock,
    &s_diary,
    &s_cube,
    &s_tide,
    &s_waves,
};

bool AppShell::begin(Display& display) {
    _display = &display;
    _features = s_registry;
    _feature_count = sizeof(s_registry) / sizeof(s_registry[0]);
    _active_index = 0;
    _needs_draw = true;

    _features[_active_index]->onEnter();
    Serial.printf("[APP] feature: %s\n", _features[_active_index]->name());
    drawActiveIfDirty();
    return true;
}

void AppShell::switchTo(size_t index) {
    if (index >= _feature_count || index == _active_index) {
        return;
    }

    _features[_active_index]->onExit();
    _active_index = index;
    _features[_active_index]->onEnter();
    _needs_draw = true;
    _nav_cooldown_until_ms = millis() + GESTURE_ECHO_COOLDOWN_MS;

    Serial.printf("[APP] feature: %s\n", _features[_active_index]->name());
}

void AppShell::handleInput(InputEvent event) {
    if (event == InputEvent::None || !_display) {
        return;
    }

    Feature* active = _features[_active_index];

    if (event == InputEvent::Tap || event == InputEvent::DoubleTap) {
        if (active->onInput(event)) {
            _needs_draw = true;
        }
        return;
    }

    const bool nav_swipe = event == InputEvent::SwipeLeft || event == InputEvent::SwipeRight ||
                           event == InputEvent::SwipeUp || event == InputEvent::SwipeDown;

    if (nav_swipe && active->capturesNavigation()) {
        if (active->onInput(event)) {
            _needs_draw = true;
        }
        return;
    }

    if (event == InputEvent::SwipeUp || event == InputEvent::SwipeDown) {
        if (active->onInput(event)) {
            _needs_draw = true;
            return;
        }
    }

    if (event == InputEvent::SwipeLeft || event == InputEvent::SwipeRight) {
        if (millis() < _nav_cooldown_until_ms) {
#if DEBUG_TOUCH
            Serial.println("[APP] nav swipe ignored (cooldown)");
#endif
            return;
        }
    }

    switch (event) {
        case InputEvent::SwipeLeft:
            switchTo((_active_index + 1) % _feature_count);
            break;
        case InputEvent::SwipeRight:
            switchTo((_active_index + _feature_count - 1) % _feature_count);
            break;
        case InputEvent::SwipeDown:
            switchTo(0);
            break;
        case InputEvent::SwipeUp:
            break;
        default:
            break;
    }
}

void AppShell::drawActiveIfDirty() {
    if (!_display) {
        return;
    }

    Feature* active = _features[_active_index];
    if (!_needs_draw && !active->isDirty()) {
        return;
    }

    active->onDraw(_display->gfx());
    active->clearDirty();
    _needs_draw = false;
}

void AppShell::tick(unsigned long now_ms) {
    if (!_display || _feature_count == 0) {
        return;
    }

    Feature* active = _features[_active_index];
    active->onTick(now_ms);
    drawActiveIfDirty();
}

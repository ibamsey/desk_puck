#include "touch_input.h"



#include <CST816S.h>



#include "config.h"



static CST816S touch(PIN_TP_SDA, PIN_TP_SCL, PIN_TP_RST, PIN_TP_INT);



static unsigned long s_last_swipe_ms = 0;

static unsigned long s_last_tap_ms = 0;



/** Ignore bounce on the same physical press; allow faster pairs for double-tap. */
static constexpr unsigned long kTapBounceMs = 50;
static constexpr unsigned long kDoubleTapWindowMs = 450;



static InputEvent gestureToEvent(byte id) {

    switch (id) {

        case SWIPE_UP:

            return InputEvent::SwipeUp;

        case SWIPE_DOWN:

            return InputEvent::SwipeDown;

        case SWIPE_LEFT:

            return InputEvent::SwipeLeft;

        case SWIPE_RIGHT:

            return InputEvent::SwipeRight;

        case SINGLE_CLICK:

            return InputEvent::Tap;

        case DOUBLE_CLICK:

            return InputEvent::DoubleTap;

        default:

            return InputEvent::None;

    }

}



/** CST816S often reports taps as finger-up with gesture NONE (HomePuck pattern). */

static InputEvent tapFromData(byte gesture, byte event) {

    if (gesture == DOUBLE_CLICK) {

        return InputEvent::DoubleTap;

    }

    if (gesture == SINGLE_CLICK) {

        return InputEvent::Tap;

    }

    if (gesture == NONE && event == 1) {

        return InputEvent::Tap;

    }

    return InputEvent::None;

}



static bool isSwipe(InputEvent e) {

    return e == InputEvent::SwipeUp || e == InputEvent::SwipeDown || e == InputEvent::SwipeLeft ||

           e == InputEvent::SwipeRight;

}



bool TouchInput::begin() {

    touch.begin();

    touch.disable_auto_sleep();

    touch.enable_double_click();

    _ready = true;

    Serial.printf("[TOUCH] CST816S ready (I2C SDA=%d SCL=%d), fw %u.%u.%u\n",

                  PIN_TP_SDA, PIN_TP_SCL,

                  touch.data.versionInfo[0],

                  touch.data.versionInfo[1],

                  touch.data.versionInfo[2]);

    return true;

}



bool TouchInput::poll(InputEvent& out) {

    out = InputEvent::None;

    if (!_ready) {

        return false;

    }



    if (!touch.available()) {

        return false;

    }



    const unsigned long now = millis();

    const byte gesture = touch.data.gestureID;

    const byte event = touch.data.event;



    const InputEvent tap_ev = tapFromData(gesture, event);

    if (tap_ev != InputEvent::None) {

        if (tap_ev == InputEvent::DoubleTap) {

            s_last_tap_ms = 0;

            out = InputEvent::DoubleTap;

#if DEBUG_TOUCH

            Serial.printf("[TOUCH] double tap gesture=%u\n", (unsigned)gesture);

#endif

            return true;

        }

        if (s_last_tap_ms != 0) {

            const unsigned long dt = now - s_last_tap_ms;

            if (dt < kTapBounceMs) {

                return false;

            }

            if (dt <= kDoubleTapWindowMs) {

                s_last_tap_ms = 0;

                out = InputEvent::DoubleTap;

#if DEBUG_TOUCH

                Serial.printf("[TOUCH] double tap (two taps, dt=%lu)\n", dt);

#endif

                return true;

            }

        }

        s_last_tap_ms = now;

        out = InputEvent::Tap;

#if DEBUG_TOUCH

        Serial.printf("[TOUCH] tap gesture=%u event=%u\n", (unsigned)gesture, (unsigned)event);

#endif

        return true;

    }



    InputEvent swipe_ev = gestureToEvent(gesture);

    if (!isSwipe(swipe_ev)) {

        return false;

    }



    if (now - s_last_swipe_ms < TOUCH_GESTURE_DEBOUNCE_MS) {

        return false;

    }



    s_last_swipe_ms = now;

    _last_gesture_ms = now;

    out = swipe_ev;



#if DEBUG_TOUCH

    Serial.printf("[TOUCH] gesture id=%u\n", (unsigned)gesture);

#endif



    return true;

}



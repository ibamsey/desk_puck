#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

#include "input_event.h"

class TouchInput {
public:
    bool begin();
    /** @return true if a non-None event was written to out */
    bool poll(InputEvent& out);

private:
    bool _ready = false;
    unsigned long _last_gesture_ms = 0;
};

#endif // TOUCH_INPUT_H

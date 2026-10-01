#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "LGFX_config.h"

class Display {
public:
    bool begin();
    lgfx::LGFX_Device& gfx() { return _lcd; }

private:
    LGFX _lcd;
    bool _ready = false;
};

#endif // DISPLAY_H

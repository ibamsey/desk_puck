#include "display.h"
#include "config.h"

bool Display::begin() {
    if (!_lcd.init()) {
        return false;
    }
    _lcd.setBrightness(200);
    _lcd.fillScreen(TFT_BLACK);
    _ready = true;
    return true;
}


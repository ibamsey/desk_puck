#include "patterns/patterns_feature.h"

#include "config.h"

#include <esp_random.h>

void PatternsFeature::regenerate() {
    _seed = esp_random();
    setDirty();
}

void PatternsFeature::onEnter() {
    regenerate();
}

bool PatternsFeature::onInput(InputEvent event) {
    if (event == InputEvent::Tap || event == InputEvent::DoubleTap) {
        regenerate();
        return true;
    }
    return false;
}

void PatternsFeature::onDraw(lgfx::LGFX_Device& gfx) {
    const int cx = DISPLAY_WIDTH / 2;
    const int cy = DISPLAY_HEIGHT / 2;

    uint32_t state = _seed ? _seed : 1;

    auto next = [&state]() -> uint32_t {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    };

    gfx.fillScreen(TFT_BLACK);

    for (int i = 0; i < 24; i++) {
        uint32_t r = next();
        int x = (int)(r % DISPLAY_WIDTH);
        int y = (int)((r >> 8) % DISPLAY_HEIGHT);
        int rad = 8 + (int)((r >> 16) % 40);
        uint16_t color = (uint16_t)(r & 0xFFFF);
        gfx.fillCircle(x, y, rad, color);
    }

    for (int i = 0; i < 12; i++) {
        uint32_t r = next();
        int x0 = (int)(r % DISPLAY_WIDTH);
        int y0 = (int)((r >> 8) % DISPLAY_HEIGHT);
        int x1 = (int)((r >> 16) % DISPLAY_WIDTH);
        int y1 = (int)((r >> 24) % DISPLAY_HEIGHT);
        gfx.drawLine(x0, y0, x1, y1, (uint16_t)(r & 0xFFFF));
    }

    gfx.drawCircle(cx, cy, 118, TFT_WHITE);

    gfx.setTextDatum(textdatum_t::middle_center);
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.setTextSize(2);
    gfx.drawString("Patterns", cx, cy);
    gfx.setTextSize(1);
    gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
    gfx.drawString("tap = new", cx, cy + 22);
}

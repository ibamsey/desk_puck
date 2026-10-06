#include "waves/wave_backdrop.h"

#include "config.h"
#include "waves/wave_style.h"

void wave_backdrop_fill(lgfx::LovyanGFX& gfx, int width, int height) {
    (void)width;
    (void)height;
    const int cx = DISPLAY_WIDTH / 2;
    const int cy = DISPLAY_HEIGHT / 2;
    const int r = DISPLAY_WIDTH / 2;

    gfx.fillScreen(wave_style::kOutside);
    gfx.fillCircle(cx, cy, r, wave_style::kSky);
}

void wave_backdrop_draw_hud(lgfx::LovyanGFX& gfx, float hs_m, float tp_s) {
    wave_style::draw_hud(gfx, hs_m, tp_s);
}

#include "waves/wave_style.h"

#include "config.h"

#include <stdio.h>

namespace wave_style {

namespace {

constexpr int kCx = DISPLAY_WIDTH / 2;
constexpr int kCy = DISPLAY_HEIGHT / 2;

} // namespace

void draw_hud(lgfx::LovyanGFX& gfx, float hs_m, float tp_s) {
    char buf_h[24];
    char buf_t[24];
    snprintf(buf_h, sizeof(buf_h), "%.2f m", (double)hs_m);
    snprintf(buf_t, sizeof(buf_t), "%.1f s", (double)tp_s);

    constexpr int kHudY = 38;
    gfx.setTextDatum(textdatum_t::top_center);
    gfx.setTextSize(1);
    gfx.setTextColor(kTextSecondary);
    gfx.drawString("Hs", kCx - 48, kHudY);
    gfx.drawString("Tp", kCx + 48, kHudY);

    gfx.setTextSize(2);
    gfx.setTextColor(kTextPrimary);
    gfx.drawString(buf_h, kCx - 48, kHudY + 14);
    gfx.drawString(buf_t, kCx + 48, kHudY + 14);
}

} // namespace wave_style

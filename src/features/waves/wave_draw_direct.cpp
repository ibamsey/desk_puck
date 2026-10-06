#include "waves/wave_draw_direct.h"

#include "config.h"
#include "waves/wave_backdrop.h"
#include "waves/wave_raster.h"
#include "waves/wave_style.h"

void wave_draw_direct(lgfx::LGFX_Device& display, float hs_m, float tp_s, unsigned long anim_origin_ms,
                      unsigned long now_ms) {
    WaveRasterParams rp;
    rp.hs_m = hs_m;
    rp.tp_s = tp_s;
    rp.time_s = (double)(now_ms - anim_origin_ms) * 0.001;

    float surface_y[DISPLAY_WIDTH];
    wave_raster_surface_y(rp, DISPLAY_WIDTH, surface_y);

    display.waitDisplay();
    display.startWrite();
    wave_backdrop_fill(display, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    wave_raster_paint_sea(display, DISPLAY_WIDTH, DISPLAY_HEIGHT, surface_y);
    display.endWrite();

    wave_backdrop_draw_hud(display, hs_m, tp_s);
}

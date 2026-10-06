#ifndef WAVES_WAVE_RASTER_H
#define WAVES_WAVE_RASTER_H

#include "LGFX_config.h"
#include <stdint.h>

struct WaveRasterParams {
    float hs_m = 0.0f;
    float tp_s = 3.0f;
    double time_s = 0.0;
};

void wave_raster_surface_y(const WaveRasterParams& params, int width, float* y_out);

/** Sea fill below the surface curve (expects sky disc already drawn). */
void wave_raster_paint_sea(lgfx::LovyanGFX& gfx, int width, int height, const float* surface_y);

#endif // WAVES_WAVE_RASTER_H

#ifndef WAVES_WAVE_DRAW_DIRECT_H
#define WAVES_WAVE_DRAW_DIRECT_H

#include "LGFX_config.h"
#include "waves/wave_raster.h"

/** Fallback path when the full frame buffer cannot be allocated. */
void wave_draw_direct(lgfx::LGFX_Device& display, float hs_m, float tp_s, unsigned long anim_origin_ms,
                      unsigned long now_ms);

#endif // WAVES_WAVE_DRAW_DIRECT_H

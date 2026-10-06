#ifndef WAVES_WAVE_STYLE_H
#define WAVES_WAVE_STYLE_H

#include "LGFX_config.h"
#include <stdint.h>

namespace wave_style {

/** LovyanGFX palette colours (same values as tide gauge / direct draw — not raw buffer packing). */
constexpr uint16_t kSky = 0x0841;
constexpr uint16_t kSea = 0x001F;
constexpr uint16_t kOutside = 0x0000;
constexpr uint16_t kTextPrimary = 0xFFFF;
constexpr uint16_t kTextSecondary = 0xC618;

void draw_hud(lgfx::LovyanGFX& gfx, float hs_m, float tp_s);

} // namespace wave_style

#endif // WAVES_WAVE_STYLE_H

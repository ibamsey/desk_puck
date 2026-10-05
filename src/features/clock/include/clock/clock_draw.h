#ifndef CLOCK_CLOCK_DRAW_H
#define CLOCK_CLOCK_DRAW_H

#include "config.h"
#include "LGFX_config.h"

namespace clock_draw {

constexpr int kCenterX = DISPLAY_WIDTH / 2;
constexpr int kCenterY = DISPLAY_HEIGHT / 2;
constexpr int kDialRadius = 98;

float deg_to_rad(float deg);
void polar_to_xy(int cx, int cy, float angle_deg, float radius, int& x, int& y);

void fill_radial_disc(lgfx::LovyanGFX& lcd, int cx, int cy, int radius, uint16_t inner_rgb,
                      uint16_t outer_rgb);

void draw_tick_ring(lgfx::LovyanGFX& lcd, int cx, int cy, int radius, uint16_t major_color,
                    uint16_t minor_color, int major_len, int minor_len, int major_w, int minor_w);

void draw_hand_line(lgfx::LovyanGFX& lcd, int cx, int cy, float angle_deg, float length, int width,
                    uint16_t color, bool rounded_cap = true);

void draw_hand_triangle(lgfx::LovyanGFX& lcd, int cx, int cy, float angle_deg, float length,
                        float width_at_base, uint16_t color);

void draw_second_hand(lgfx::LovyanGFX& lcd, int cx, int cy, float angle_deg, float tail_len,
                      float tip_len, uint16_t color);

} // namespace clock_draw

#endif // CLOCK_CLOCK_DRAW_H

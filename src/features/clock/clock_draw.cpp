#include "clock/clock_draw.h"

#include <cmath>

namespace clock_draw {

float deg_to_rad(float deg) {
    return deg * (float)M_PI / 180.0f;
}

void polar_to_xy(int cx, int cy, float angle_deg, float radius, int& x, int& y) {
    const float rad = deg_to_rad(angle_deg - 90.0f);
    x = cx + (int)lroundf(radius * cosf(rad));
    y = cy + (int)lroundf(radius * sinf(rad));
}

static uint16_t lerp_rgb565(uint16_t a, uint16_t b, float t) {
    const uint8_t ar = (a >> 11) & 0x1F;
    const uint8_t ag = (a >> 5) & 0x3F;
    const uint8_t ab = a & 0x1F;
    const uint8_t br = (b >> 11) & 0x1F;
    const uint8_t bg = (b >> 5) & 0x3F;
    const uint8_t bb = b & 0x1F;
    const uint8_t rr = (uint8_t)(br + (ar - br) * t);
    const uint8_t rg = (uint8_t)(bg + (ag - bg) * t);
    const uint8_t rb = (uint8_t)(bb + (ab - bb) * t);
    return (uint16_t)((rr << 11) | (rg << 5) | rb);
}

void fill_radial_disc(LGFX_Device& lcd, int cx, int cy, int radius,
                      uint16_t inner_rgb, uint16_t outer_rgb) {
    for (int r = radius; r >= 0; r -= 2) {
        const float t = (float)r / (float)radius;
        lcd.fillCircle(cx, cy, r, lerp_rgb565(inner_rgb, outer_rgb, t));
    }
}

void draw_tick_ring(LGFX_Device& lcd, int cx, int cy, int radius,
                    uint16_t major_color, uint16_t minor_color,
                    int major_len, int minor_len, int major_w, int minor_w) {
    for (int i = 0; i < 60; ++i) {
        const bool major = (i % 5) == 0;
        const float angle = (float)i * 6.0f;
        const int len = major ? major_len : minor_len;
        const int w = major ? major_w : minor_w;
        const uint16_t color = major ? major_color : minor_color;
        int x0, y0, x1, y1;
        polar_to_xy(cx, cy, angle, (float)(radius - len), x0, y0);
        polar_to_xy(cx, cy, angle, (float)radius, x1, y1);
        lcd.drawWideLine(x0, y0, x1, y1, w, color);
    }
}

void draw_hand_line(LGFX_Device& lcd, int cx, int cy, float angle_deg, float length,
                    int width, uint16_t color, bool rounded_cap) {
    int x2, y2;
    polar_to_xy(cx, cy, angle_deg, length, x2, y2);
    if (width <= 1) {
        lcd.drawLine(cx, cy, x2, y2, color);
        return;
    }
    lcd.drawWideLine(cx, cy, x2, y2, width, color);
    if (rounded_cap) {
        lcd.fillCircle(x2, y2, width / 2, color);
    }
}

void draw_hand_triangle(LGFX_Device& lcd, int cx, int cy, float angle_deg, float length,
                        float width_at_base, uint16_t color) {
    int tip_x, tip_y;
    polar_to_xy(cx, cy, angle_deg, length, tip_x, tip_y);

    const float perp = angle_deg + 90.0f;
    int bx, by, cx2, cy2;
    polar_to_xy(cx, cy, perp, width_at_base * 0.5f, bx, by);
    polar_to_xy(cx, cy, perp + 180.0f, width_at_base * 0.5f, cx2, cy2);
    lcd.fillTriangle(cx, cy, bx, by, tip_x, tip_y, color);
    lcd.fillTriangle(cx, cy, cx2, cy2, tip_x, tip_y, color);
}

void draw_second_hand(LGFX_Device& lcd, int cx, int cy, float angle_deg, float tail_len,
                      float tip_len, uint16_t color) {
    int tail_x, tail_y, tip_x, tip_y;
    polar_to_xy(cx, cy, angle_deg + 180.0f, tail_len, tail_x, tail_y);
    polar_to_xy(cx, cy, angle_deg, tip_len, tip_x, tip_y);
    lcd.drawLine(tail_x, tail_y, tip_x, tip_y, color);
    lcd.fillCircle(tip_x, tip_y, 2, color);
}

} // namespace clock_draw

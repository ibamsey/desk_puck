#include "waves/wave_raster.h"

#include "config.h"
#include "waves/wave_style.h"

#include <math.h>

namespace {

constexpr int kCx = DISPLAY_WIDTH / 2;
constexpr int kCy = DISPLAY_HEIGHT / 2;
constexpr int kR = DISPLAY_WIDTH / 2;
constexpr float kPi = 3.14159265f;
constexpr float kDefaultHalfRangeM = 1.0f;

int circle_bottom_y(int x) {
    const int dx = x - kCx;
    const int dx2 = dx * dx;
    if (dx2 > kR * kR) {
        return -1;
    }
    const int dy = (int)sqrtf((float)(kR * kR - dx2));
    return kCy + dy;
}

float half_range_m(float hs_m) {
    const float amp = hs_m * 0.5f;
    if (amp <= kDefaultHalfRangeM) {
        return kDefaultHalfRangeM;
    }
    return amp * 1.08f;
}

float wave_eta_at_x(float x_px, float amp_m, float period_s, double time_s) {
    if (period_s <= 0.05f) {
        return 0.0f;
    }
    const double t_in_period = fmod(time_s, (double)period_s) / (double)period_s;
    const double x_norm = (double)x_px / (double)DISPLAY_WIDTH;
    const double phase = 2.0 * (double)kPi * (x_norm - t_in_period);
    return amp_m * (float)sin(phase);
}

float surface_y_at(const float* surface_y, int width, float x) {
    if (x <= 0.0f) {
        return surface_y[0];
    }
    if (x >= (float)(width - 1)) {
        return surface_y[width - 1];
    }
    const int x0 = (int)floorf(x);
    const float f = x - (float)x0;
    return surface_y[x0] * (1.0f - f) + surface_y[x0 + 1] * f;
}

} // namespace

void wave_raster_surface_y(const WaveRasterParams& params, int width, float* y_out) {
    const float amp_m = params.hs_m * 0.5f;
    const float period_s = params.tp_s > 0.05f ? params.tp_s : 3.0f;
    const float half_m = half_range_m(params.hs_m);
    const float ppm = (float)kR / half_m;

    for (int x = 0; x < width; x++) {
        const float x_center = (float)x + 0.5f;
        const float eta = wave_eta_at_x(x_center, amp_m, period_s, params.time_s);
        float y = (float)kCy - eta * ppm;
        if (y < 0.0f) {
            y = 0.0f;
        }
        if (y > (float)(DISPLAY_HEIGHT - 1)) {
            y = (float)(DISPLAY_HEIGHT - 1);
        }
        y_out[x] = y;
    }
}

void wave_raster_paint_sea(lgfx::LovyanGFX& gfx, int width, int height, const float* surface_y) {
    (void)height;
    const uint16_t sea = wave_style::kSea;

    for (int x = 0; x < width; x++) {
        const int y_bot = circle_bottom_y(x);
        if (y_bot < 0) {
            continue;
        }

        const float ys = surface_y_at(surface_y, width, (float)x + 0.5f);
        const int y0 = (int)ceilf(ys - 0.5f) + 1;
        if (y0 > y_bot) {
            continue;
        }

        const int h = y_bot - y0 + 1;
        if (h > 0) {
            gfx.fillRect(x, y0, 1, h, sea);
        }
    }
}

#include "waves/wave_compositor.h"

#include "config.h"
#include "waves/wave_backdrop.h"
#include "waves/wave_draw_direct.h"
#include "waves/wave_raster.h"

#include <stdlib.h>

namespace {

constexpr size_t kFramePixels = (size_t)DISPLAY_WIDTH * (size_t)DISPLAY_HEIGHT;
constexpr size_t kFrameBytes = kFramePixels * sizeof(uint16_t);

} // namespace

void WaveCompositor::reset() {
    if (_frame) {
        free(_frame);
        _frame = nullptr;
    }
}

bool WaveCompositor::ensure_buffer() {
    return ensure_frame_buffer();
}

bool WaveCompositor::ensure_frame_buffer() {
    if (_frame) {
        return true;
    }
    _frame = (uint16_t*)malloc(kFrameBytes);
    if (!_frame) {
        Serial.printf("[WAVE] frame alloc failed, heap=%u\n", (unsigned)ESP.getFreeHeap());
        return false;
    }
    Serial.printf("[WAVE] frame buffer ok, heap=%u\n", (unsigned)ESP.getFreeHeap());
    return true;
}

void WaveCompositor::present_full_frame(lgfx::LGFX_Device& display, float hs_m, float tp_s,
                                        unsigned long anim_origin_ms, unsigned long now_ms) {
    WaveRasterParams rp;
    rp.hs_m = hs_m;
    rp.tp_s = tp_s;
    rp.time_s = (double)(now_ms - anim_origin_ms) * 0.001;

    float surface_y[DISPLAY_WIDTH];
    wave_raster_surface_y(rp, DISPLAY_WIDTH, surface_y);

    lgfx::LGFX_Sprite layer(&display);
    layer.setColorDepth(lgfx::color_depth_t::rgb565_2Byte);
    layer.setBuffer(_frame, DISPLAY_WIDTH, DISPLAY_HEIGHT, lgfx::color_depth_t::rgb565_2Byte);

    wave_backdrop_fill(layer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    wave_raster_paint_sea(layer, DISPLAY_WIDTH, DISPLAY_HEIGHT, surface_y);

    display.waitDisplay();
    display.startWrite();
    display.pushImage(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, _frame);
    display.endWrite();

    wave_backdrop_draw_hud(display, hs_m, tp_s);
}

void WaveCompositor::present_direct(lgfx::LGFX_Device& display, float hs_m, float tp_s,
                                    unsigned long anim_origin_ms, unsigned long now_ms) {
    wave_draw_direct(display, hs_m, tp_s, anim_origin_ms, now_ms);
}

void WaveCompositor::present(lgfx::LGFX_Device& display, float hs_m, float tp_s,
                              unsigned long anim_origin_ms, unsigned long now_ms) {
    if (ensure_frame_buffer()) {
        present_full_frame(display, hs_m, tp_s, anim_origin_ms, now_ms);
        return;
    }
    present_direct(display, hs_m, tp_s, anim_origin_ms, now_ms);
}

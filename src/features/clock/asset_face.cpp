#include "clock/asset_face.h"

#include "clock/chronograph.h"
#include "config.h"

#include <LovyanGFX.hpp>

#include <cmath>
#include <cstring>

namespace {

constexpr size_t kPackHeaderSize = 12;
constexpr size_t kHandChunkHeaderSize = 10;
constexpr size_t kDialBytes = 240 * 240 * 2;
constexpr uint32_t kMagic = 0x4B435057;
constexpr int kAaPad = 4;

uint8_t rgb565_to8(uint16_t c, int shift, int bits) {
    const uint16_t mask = (1u << bits) - 1;
    return (uint8_t)(((c >> shift) & mask) * 255 / mask);
}

lgfx::argb8888_t pack_pixel_to_argb(uint16_t rgb565, uint8_t alpha) {
    if (alpha == 0) {
        return lgfx::argb8888_t(0, 0, 0, 0);
    }
    const uint8_t r = rgb565_to8(rgb565, 11, 5);
    const uint8_t g = rgb565_to8(rgb565, 5, 6);
    const uint8_t b = rgb565_to8(rgb565, 0, 5);
    return lgfx::argb8888_t(alpha, r, g, b);
}

void swap_rgb565_buffer(uint16_t* pixels, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const uint16_t c = pixels[i];
        pixels[i] = (uint16_t)((c >> 8) | (c << 8));
    }
}

void make_lgfx_rotation_matrix(float* m, float dst_x, float dst_y, float src_x, float src_y,
                               float angle_deg, float zoom_x, float zoom_y) {
    float rad = fmodf(angle_deg, 360.0f) * (float)M_PI / 180.0f;
    const float sin_f = sinf(rad);
    const float cos_f = cosf(rad);
    m[0] = cos_f * zoom_x;
    m[1] = -sin_f * zoom_y;
    m[2] = dst_x - src_x * m[0] - src_y * m[1];
    m[3] = sin_f * zoom_x;
    m[4] = cos_f * zoom_y;
    m[5] = dst_y - src_x * m[3] - src_y * m[4];
}

float bake_hour_angle(int hour, int minute) {
    return (float)(hour % 12) * 30.0f + (float)minute * 0.5f;
}

float bake_minute_angle(int minute) {
    return (float)minute * 6.0f;
}

} // namespace

void AssetFaceRuntime::draw_hand(lgfx::LovyanGFX& gfx, const HandBuffer& hand, float angle_deg) const {
    draw_hand_at(gfx, hand, _pivot_x, _pivot_y, angle_deg);
}

void AssetFaceRuntime::draw_hand_at(lgfx::LovyanGFX& gfx, const HandBuffer& hand, int pivot_x, int pivot_y,
                                    float angle_deg) const {
    if (!hand.active || !hand.pixels) {
        return;
    }
    composite_hand_on_panel(static_cast<lgfx::LGFX_Device&>(gfx), hand, pivot_x, pivot_y, angle_deg, false,
                            0.0f);
}

void AssetFaceRuntime::clip_rect(int& x, int& y, int& w, int& h) const {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > 240) {
        w = 240 - x;
    }
    if (y + h > 240) {
        h = 240 - y;
    }
}

bool AssetFaceRuntime::ensure_hand_patch(size_t pixel_count) const {
    if (_hand_patch && _hand_patch_pixels >= pixel_count) {
        return true;
    }
    if (_hand_patch) {
        free(_hand_patch);
        _hand_patch = nullptr;
        _hand_patch_pixels = 0;
    }
    _hand_patch = (uint16_t*)malloc(pixel_count * sizeof(uint16_t));
    if (!_hand_patch) {
        return false;
    }
    _hand_patch_pixels = pixel_count;
    return true;
}

void AssetFaceRuntime::composite_hand_on_panel(lgfx::LGFX_Device& gfx, const HandBuffer& hand, int pivot_x,
                                               int pivot_y, float angle_deg, bool has_prev,
                                               float prev_angle_deg) const {
    if (!hand.active || !hand.pixels || !_static_valid || !_static) {
        return;
    }
    int ux = 0;
    int uy = 0;
    int uw = 0;
    int uh = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    if (has_prev) {
        hand_dirty_rect(pivot_x, pivot_y, hand, prev_angle_deg, x, y, w, h);
        rect_union(ux, uy, uw, uh, x, y, w, h);
    }
    hand_dirty_rect(pivot_x, pivot_y, hand, angle_deg, x, y, w, h);
    rect_union(ux, uy, uw, uh, x, y, w, h);
    clip_rect(ux, uy, uw, uh);
    if (uw <= 0 || uh <= 0) {
        return;
    }
    const size_t patch_pixels = (size_t)uw * (size_t)uh;
    if (!ensure_hand_patch(patch_pixels)) {
        return;
    }
    for (int row = 0; row < uh; ++row) {
        memcpy(&_hand_patch[(size_t)row * (size_t)uw], &_static[(uy + row) * 240 + ux], (size_t)uw * 2);
    }

    lgfx::LGFX_Sprite layer;
    layer.setColorDepth(lgfx::color_depth_t::rgb565_2Byte);
    layer.setBuffer(_hand_patch, uw, uh, lgfx::color_depth_t::rgb565_2Byte);
    const float angle = angle_deg + hand.offset_deg;
    layer.pushImageRotateZoomWithAA(pivot_x - ux, pivot_y - uy, hand.pivot_x, hand.pivot_y, angle, 1.0f, 1.0f,
                                    hand.width, hand.height, hand.pixels, 0u);
    gfx.pushImage(ux, uy, uw, uh, _hand_patch);
}

void AssetFaceRuntime::draw_hub(lgfx::LovyanGFX& gfx) const {
    if (!_has_hub || !_hub.active || !_hub.pixels) {
        return;
    }
    const int x = _pivot_x - _hub.pivot_x;
    const int y = _pivot_y - _hub.pivot_y;
    gfx.pushImage(x, y, _hub.width, _hub.height, _hub.pixels, 0u);
}

float AssetFaceRuntime::subdial_angle(SubdialRole role, const AnalogClockState& state) const {
    switch (role) {
        case SubdialRole::WallSecond:
            return state.second_angle;
        case SubdialRole::ChronoSecond:
            return chronograph_second_angle(state.chrono_elapsed_ms);
        case SubdialRole::ChronoMinute:
            return chronograph_minute_angle(state.chrono_elapsed_ms);
        default:
            return 0.0f;
    }
}

void AssetFaceRuntime::draw_fast_hands(lgfx::LGFX_Device& gfx, const AnalogClockState& state,
                                       bool partial_update) const {
    if (_has_second) {
        if (partial_update && _has_shown_second) {
            composite_hand_on_panel(gfx, _second, _pivot_x, _pivot_y, state.second_angle, true,
                                      _shown_second_angle);
        } else {
            composite_hand_on_panel(gfx, _second, _pivot_x, _pivot_y, state.second_angle, false, 0.0f);
        }
    }
    if (!state.draw_chrono) {
        return;
    }
    AnalogClockState prev = state;
    prev.chrono_elapsed_ms = _has_shown_chrono ? _shown_chrono_ms : state.chrono_elapsed_ms;
    for (uint8_t i = 0; i < _subdial_count; ++i) {
        const LoadedSubdial& sd = _subdials[i];
        if (!sd.hand.active) {
            continue;
        }
        const float a1 = subdial_angle(sd.role, state);
        if (partial_update && _has_shown_chrono) {
            const float a0 = subdial_angle(sd.role, prev);
            composite_hand_on_panel(gfx, sd.hand, sd.x, sd.y, a1, true, a0);
        } else {
            composite_hand_on_panel(gfx, sd.hand, sd.x, sd.y, a1, false, 0.0f);
        }
    }
}

void AssetFaceRuntime::blit_dial_from_flash(lgfx::LGFX_Device& gfx) const {
    if (!_blob) {
        return;
    }
    uint16_t row[240];
    for (int y = 0; y < 240; ++y) {
        memcpy_P(row, _blob + kPackHeaderSize + (size_t)y * 480, 480);
        swap_rgb565_buffer(row, 240);
        gfx.pushImage(0, y, 240, 1, row);
    }
}

bool AssetFaceRuntime::ensure_static() {
    if (_static) {
        return true;
    }
    _static = (uint16_t*)malloc(kDialBytes);
    if (!_static) {
        Serial.println("[ASSET] static layer unavailable (1 Hz flash fallback)");
        return false;
    }
    _static_valid = false;
    return true;
}

bool AssetFaceRuntime::rebuild_static(const AnalogClockState& state) {
    if (!_blob || !ensure_static()) {
        _static_valid = false;
        return false;
    }
    memcpy_P(_static, _blob + kPackHeaderSize, kDialBytes);
    swap_rgb565_buffer(_static, 240u * 240u);

    lgfx::LGFX_Sprite layer;
    layer.setColorDepth(lgfx::color_depth_t::rgb565_2Byte);
    layer.setBuffer(_static, 240, 240, lgfx::color_depth_t::rgb565_2Byte);
    auto draw_on_static = [&](const HandBuffer& hand, float angle_deg) {
        if (!hand.active || !hand.pixels) {
            return;
        }
        const float angle = angle_deg + hand.offset_deg;
        layer.pushImageRotateZoomWithAA(_pivot_x, _pivot_y, hand.pivot_x, hand.pivot_y, angle, 1.0f, 1.0f,
                                        hand.width, hand.height, hand.pixels, 0u);
    };
    draw_on_static(_hour, bake_hour_angle(state.hour, state.minute));
    draw_on_static(_minute, bake_minute_angle(state.minute));
    if (_has_hub) {
        draw_hub(layer);
    }

    _static_valid = true;
    _static_hour = state.hour;
    _static_minute = state.minute;
    return true;
}

void AssetFaceRuntime::hand_dirty_rect(int pivot_x, int pivot_y, const HandBuffer& hand, float angle_deg,
                                       int& out_x, int& out_y, int& out_w, int& out_h) const {
    if (!hand.active || hand.width == 0 || hand.height == 0) {
        out_x = out_y = out_w = out_h = 0;
        return;
    }
    const float rot = angle_deg + hand.offset_deg;
    float m[6];
    make_lgfx_rotation_matrix(m, (float)pivot_x + 0.5f, (float)pivot_y + 0.5f, (float)hand.pivot_x + 0.5f,
                              (float)hand.pivot_y + 0.5f, rot, 1.0f, 1.0f);
    const float w = (float)hand.width;
    const float h = (float)hand.height;
    const float corners[4][2] = {{0.0f, 0.0f}, {w, 0.0f}, {w, h}, {0.0f, h}};
    float min_x = m[0] * corners[0][0] + m[1] * corners[0][1] + m[2];
    float min_y = m[3] * corners[0][0] + m[4] * corners[0][1] + m[5];
    float max_x = min_x;
    float max_y = min_y;
    for (int i = 1; i < 4; ++i) {
        const float dx = m[0] * corners[i][0] + m[1] * corners[i][1] + m[2];
        const float dy = m[3] * corners[i][0] + m[4] * corners[i][1] + m[5];
        if (dx < min_x) {
            min_x = dx;
        }
        if (dy < min_y) {
            min_y = dy;
        }
        if (dx > max_x) {
            max_x = dx;
        }
        if (dy > max_y) {
            max_y = dy;
        }
    }
    out_x = (int)floorf(min_x) - kAaPad;
    out_y = (int)floorf(min_y) - kAaPad;
    out_w = (int)ceilf(max_x - min_x) + kAaPad * 2;
    out_h = (int)ceilf(max_y - min_y) + kAaPad * 2;
}

void AssetFaceRuntime::rect_union(int& ux, int& uy, int& uw, int& uh, int x, int y, int w, int h) const {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (uw <= 0 || uh <= 0) {
        ux = x;
        uy = y;
        uw = w;
        uh = h;
        return;
    }
    const int x2 = x + w;
    const int y2 = y + h;
    const int ux2 = ux + uw;
    const int uy2 = uy + uh;
    ux = (ux < x) ? ux : x;
    uy = (uy < y) ? uy : y;
    uw = ((ux2 > x2) ? ux2 : x2) - ux;
    uh = ((uy2 > y2) ? uy2 : y2) - uy;
}

void AssetFaceRuntime::remember_shown(const AnalogClockState& state) {
    _has_shown_second = true;
    _shown_second_angle = state.second_angle;
    if (state.draw_chrono) {
        _has_shown_chrono = true;
        _shown_chrono_ms = state.chrono_elapsed_ms;
    }
}

void AssetFaceRuntime::unload() {
    if (_static) {
        free(_static);
        _static = nullptr;
    }
    if (_hand_patch) {
        free(_hand_patch);
        _hand_patch = nullptr;
        _hand_patch_pixels = 0;
    }
    _static_valid = false;
    _static_hour = -1;
    _static_minute = -1;
    auto free_hand = [](HandBuffer& h) {
        if (h.pixels) {
            free(h.pixels);
            h.pixels = nullptr;
        }
        h.active = false;
    };
    free_hand(_hour);
    free_hand(_minute);
    free_hand(_second);
    free_hand(_hub);
    for (uint8_t i = 0; i < _subdial_count; ++i) {
        free_hand(_subdials[i].hand);
    }
    _subdial_count = 0;
    _blob = nullptr;
    _asset_index = -1;
    _has_second = false;
    _has_hub = false;
    _has_shown_second = false;
    _has_shown_chrono = false;
}

bool AssetFaceRuntime::load_hand_chunk(const uint8_t* blob, size_t& offset, const WatchFaceHandMeta& meta,
                                       HandBuffer& out) {
    uint16_t w = 0;
    uint16_t h = 0;
    int16_t px = 0;
    int16_t py = 0;
    uint16_t reserved = 0;
    memcpy_P(&w, blob + offset, 2);
    memcpy_P(&h, blob + offset + 2, 2);
    memcpy_P(&px, blob + offset + 4, 2);
    memcpy_P(&py, blob + offset + 6, 2);
    memcpy_P(&reserved, blob + offset + 8, 2);
    (void)reserved;

    if (w != meta.width || h != meta.height || px != meta.pivot_x || py != meta.pivot_y) {
        Serial.printf("[ASSET] hand chunk mismatch kind=%u\n", (unsigned)meta.kind);
        return false;
    }

    offset += kHandChunkHeaderSize;
    const size_t pix_bytes = (size_t)w * (size_t)h * 3;
    const size_t argb_bytes = (size_t)w * (size_t)h * sizeof(lgfx::argb8888_t);
    lgfx::argb8888_t* pixels = (lgfx::argb8888_t*)malloc(argb_bytes);
    if (!pixels) {
        return false;
    }

    for (size_t i = 0; i < (size_t)w * (size_t)h; ++i) {
        uint8_t lo = pgm_read_byte(blob + offset + i * 3);
        uint8_t hi = pgm_read_byte(blob + offset + i * 3 + 1);
        uint8_t a = pgm_read_byte(blob + offset + i * 3 + 2);
        const uint16_t rgb565 = (uint16_t)(lo | (hi << 8));
        pixels[i] = pack_pixel_to_argb(rgb565, a);
    }
    offset += pix_bytes;

    out.pixels = pixels;
    out.width = w;
    out.height = h;
    out.pivot_x = px;
    out.pivot_y = py;
    out.offset_deg = meta.offset_deg;
    out.active = true;
    return true;
}

bool AssetFaceRuntime::load_hub_chunk(const uint8_t* blob, size_t& offset, const WatchFaceHubMeta& meta,
                                      HandBuffer& out) {
    WatchFaceHandMeta hm{};
    hm.width = meta.width;
    hm.height = meta.height;
    hm.pivot_x = meta.pivot_x;
    hm.pivot_y = meta.pivot_y;
    hm.offset_deg = 0;
    hm.kind = WatchFaceHandKind::Hour;
    return load_hand_chunk(blob, offset, hm, out);
}

bool AssetFaceRuntime::load(int asset_index, const AssetFaceMeta* meta) {
    unload();
    if (!meta || !meta->blob) {
        return false;
    }

    uint32_t magic = 0;
    memcpy_P(&magic, meta->blob, 4);
    if (magic != kMagic) {
        Serial.println("[ASSET] bad magic");
        return false;
    }

    _blob = meta->blob;
    _asset_index = asset_index;
    _pivot_x = meta->pivot_x;
    _pivot_y = meta->pivot_y;
    _has_second = meta->has_second();
    _has_hub = meta->has_hub();
    _subdial_count = meta->subdial_count;

    size_t offset = kPackHeaderSize + kDialBytes;
    if (!load_hand_chunk(meta->blob, offset, meta->hour, _hour) ||
        !load_hand_chunk(meta->blob, offset, meta->minute, _minute)) {
        unload();
        return false;
    }
    if (_has_second) {
        if (!load_hand_chunk(meta->blob, offset, meta->second, _second)) {
            unload();
            return false;
        }
    }
    for (uint8_t i = 0; i < _subdial_count; ++i) {
        if (!load_hand_chunk(meta->blob, offset, meta->subdials[i].hand, _subdials[i].hand)) {
            unload();
            return false;
        }
        _subdials[i].x = meta->subdials[i].x;
        _subdials[i].y = meta->subdials[i].y;
        _subdials[i].role = meta->subdials[i].role;
    }
    if (_has_hub) {
        if (!load_hub_chunk(meta->blob, offset, meta->hub, _hub)) {
            unload();
            return false;
        }
    }

    if (!ensure_static()) {
        Serial.printf("[ASSET] loaded %s heap=%u (no static; 1 Hz fallback)\n", meta->id,
                      (unsigned)ESP.getFreeHeap());
    } else {
        Serial.printf("[ASSET] loaded %s heap=%u static=yes\n", meta->id, (unsigned)ESP.getFreeHeap());
    }
    return true;
}

void AssetFaceRuntime::present(lgfx::LGFX_Device& gfx, const AnalogClockState& state, bool force_full) {
    if (!_blob) {
        gfx.fillScreen(TFT_BLACK);
        gfx.setTextColor(TFT_WHITE);
        gfx.drawString("No face", 120, 120);
        return;
    }

    const bool static_stale =
        !_static_valid || state.hour != _static_hour || state.minute != _static_minute;

    if (_static && (force_full || static_stale)) {
        if (rebuild_static(state)) {
            gfx.pushImage(0, 0, 240, 240, _static);
            draw_fast_hands(gfx, state, false);
            remember_shown(state);
            return;
        }
    }

    if (_static_valid && _static && !force_full && !static_stale) {
        if (_has_second && !_has_shown_second) {
            gfx.pushImage(0, 0, 240, 240, _static);
        }
        draw_fast_hands(gfx, state, true);
        remember_shown(state);
        return;
    }

    blit_dial_from_flash(gfx);
    draw_hand(gfx, _hour, bake_hour_angle(state.hour, state.minute));
    draw_hand(gfx, _minute, bake_minute_angle(state.minute));
    if (_has_hub) {
        draw_hub(gfx);
    }
    draw_fast_hands(gfx, state, false);
    remember_shown(state);
}

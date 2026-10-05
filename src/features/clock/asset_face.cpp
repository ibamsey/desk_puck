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

static bool clock_mask_has(ClockHandMask mask, ClockHandMask bit) {
    return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(bit)) != 0;
}

static void rotate_local(float lx, float ly, float angle_deg, float& ox, float& oy) {
    const float rad = angle_deg * (float)M_PI / 180.0f;
    ox = lx * cosf(rad) + ly * sinf(rad);
    oy = -lx * sinf(rad) + ly * cosf(rad);
}

} // namespace

int AssetFaceRuntime::hand_cover_radius(const HandBuffer& hand) const {
    if (!hand.active) {
        return 0;
    }
    const float px = (float)hand.pivot_x;
    const float py = (float)hand.pivot_y;
    const float w = (float)hand.width;
    const float h = (float)hand.height;
    float max_r = 0.0f;
    const float corners[4][2] = {{0.0f, 0.0f}, {w, 0.0f}, {w, h}, {0.0f, h}};
    for (int i = 0; i < 4; ++i) {
        const float dx = corners[i][0] - px;
        const float dy = corners[i][1] - py;
        const float r = sqrtf(dx * dx + dy * dy);
        if (r > max_r) {
            max_r = r;
        }
    }
    return (int)ceilf(max_r) + 3;
}

void AssetFaceRuntime::blit_buffer_rect(lgfx::LGFX_Device& gfx, const uint16_t* src, int x, int y, int w,
                                        int h) const {
    if (!src || w <= 0 || h <= 0) {
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
    if (w <= 0 || h <= 0) {
        return;
    }
    for (int row = 0; row < h; ++row) {
        gfx.pushImage(x, y + row, w, 1, &src[(y + row) * 240 + x]);
    }
}

void AssetFaceRuntime::restore_subdial_patch(lgfx::LGFX_Device& gfx, const LoadedSubdial& sd) const {
    const int r = hand_cover_radius(sd.hand);
    if (r <= 0) {
        return;
    }
    const uint16_t* src = (_underlay_valid && _underlay) ? _underlay : _dial;
    if (!src) {
        return;
    }
    blit_buffer_rect(gfx, src, sd.x - r, sd.y - r, r * 2, r * 2);
}

bool AssetFaceRuntime::ensure_underlay() {
    if (_underlay) {
        return true;
    }
    _underlay = (uint16_t*)malloc(kDialBytes);
    if (!_underlay) {
        Serial.println("[ASSET] underlay unavailable (using dial fallback)");
        return false;
    }
    _underlay_valid = false;
    return true;
}

void AssetFaceRuntime::draw_center_wall_hands(lgfx::LovyanGFX& gfx, const AnalogClockState& state, bool hour,
                                              bool minute, bool second) const {
    if (hour) {
        draw_hand(gfx, _hour, state.hour_angle);
    }
    if (minute) {
        draw_hand(gfx, _minute, state.minute_angle);
    }
    if (second && _has_second) {
        draw_hand(gfx, _second, state.second_angle);
    }
}

void AssetFaceRuntime::rebuild_underlay(const AnalogClockState& state) {
    if (!_dial || !ensure_underlay()) {
        _underlay_valid = false;
        return;
    }
    lgfx::LGFX_Sprite layer;
    layer.setColorDepth(lgfx::color_depth_t::rgb565_2Byte);
    layer.setBuffer(_underlay, 240, 240, lgfx::color_depth_t::rgb565_2Byte);
    layer.pushImage(0, 0, 240, 240, _dial);
    draw_hand(layer, _hour, state.hour_angle);
    draw_hand(layer, _minute, state.minute_angle);
    _underlay_valid = true;
    _underlay_hour_angle = state.hour_angle;
    _underlay_minute_angle = state.minute_angle;
}

void AssetFaceRuntime::unpaint_hand_from_background(lgfx::LGFX_Device& gfx, const HandBuffer& hand, int pivot_x,
                                                    int pivot_y, float angle_deg,
                                                    const uint16_t* bg_rgb565) const {
    if (!hand.active || !hand.pixels || !bg_rgb565) {
        return;
    }
    const float rot = angle_deg + hand.offset_deg;
    const uint32_t count = (uint32_t)hand.width * (uint32_t)hand.height;
    for (uint32_t i = 0; i < count; ++i) {
        const lgfx::argb8888_t& px = hand.pixels[i];
        if (px.a < 128) {
            continue;
        }
        const uint16_t px_x = (uint16_t)(i % hand.width);
        const uint16_t px_y = (uint16_t)(i / hand.width);
        const float lx = (float)px_x - (float)hand.pivot_x;
        const float ly = (float)px_y - (float)hand.pivot_y;
        float ox, oy;
        rotate_local(lx, ly, rot, ox, oy);
        const int sx = pivot_x + (int)lroundf(ox);
        const int sy = pivot_y + (int)lroundf(oy);
        if (sx < 0 || sy < 0 || sx >= 240 || sy >= 240) {
            continue;
        }
        gfx.writePixel(sx, sy, bg_rgb565[sy * 240 + sx]);
    }
}

void AssetFaceRuntime::hand_dirty_rect(int pivot_x, int pivot_y, const HandBuffer& hand, float angle_deg,
                                       int& out_x, int& out_y, int& out_w, int& out_h) const {
    const int r = hand_cover_radius(hand);
    (void)angle_deg;
    out_x = pivot_x - r;
    out_y = pivot_y - r;
    out_w = r * 2;
    out_h = r * 2;
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
    const int nx = (ux < x) ? ux : x;
    const int ny = (uy < y) ? uy : y;
    const int nx2 = (ux2 > x2) ? ux2 : x2;
    const int ny2 = (uy2 > y2) ? uy2 : y2;
    ux = nx;
    uy = ny;
    uw = nx2 - nx;
    uh = ny2 - ny;
}

void AssetFaceRuntime::patch_underlay_hands(lgfx::LGFX_Device& gfx, const AnalogClockState& state,
                                            const AnalogClockState& prev, ClockHandMask mask) const {
    if (!_underlay_valid || !_underlay) {
        return;
    }

    int ux = 0;
    int uy = 0;
    int uw = 0;
    int uh = 0;
    bool any = false;

    auto add_hand = [&](const HandBuffer& hand, float a0, float a1) {
        if (!hand.active) {
            return;
        }
        int x0, y0, w0, h0;
        int x1, y1, w1, h1;
        hand_dirty_rect(_pivot_x, _pivot_y, hand, a0, x0, y0, w0, h0);
        hand_dirty_rect(_pivot_x, _pivot_y, hand, a1, x1, y1, w1, h1);
        rect_union(ux, uy, uw, uh, x0, y0, w0, h0);
        rect_union(ux, uy, uw, uh, x1, y1, w1, h1);
        any = true;
    };

    if (clock_mask_has(mask, ClockHandMask::Hour)) {
        add_hand(_hour, prev.hour_angle, state.hour_angle);
    }
    if (clock_mask_has(mask, ClockHandMask::Minute)) {
        add_hand(_minute, prev.minute_angle, state.minute_angle);
        add_hand(_hour, prev.hour_angle, state.hour_angle);
    }

    if (!any || uw <= 0 || uh <= 0) {
        return;
    }

    blit_buffer_rect(gfx, _underlay, ux, uy, uw, uh);
}

void AssetFaceRuntime::second_sweep_union_rect(const AnalogClockState& state, const AnalogClockState& prev,
                                               int& ux, int& uy, int& uw, int& uh) const {
    int x0, y0, w0, h0;
    int x1, y1, w1, h1;
    hand_dirty_rect(_pivot_x, _pivot_y, _second, prev.second_angle, x0, y0, w0, h0);
    hand_dirty_rect(_pivot_x, _pivot_y, _second, state.second_angle, x1, y1, w1, h1);
    ux = (x0 < x1) ? x0 : x1;
    uy = (y0 < y1) ? y0 : y1;
    const int x0e = x0 + w0;
    const int x1e = x1 + w1;
    const int y0e = y0 + h0;
    const int y1e = y1 + h1;
    uw = ((x0e > x1e) ? x0e : x1e) - ux;
    uh = ((y0e > y1e) ? y0e : y1e) - uy;
}

void AssetFaceRuntime::draw_second_sweep(lgfx::LGFX_Device& gfx, const AnalogClockState& state,
                                         const AnalogClockState& prev, bool draw_hub_cap) const {
    if (!_has_second) {
        return;
    }
    const uint16_t* bg = (_underlay_valid && _underlay) ? _underlay : _dial;
    if (!bg) {
        return;
    }
    unpaint_hand_from_background(gfx, _second, _pivot_x, _pivot_y, prev.second_angle, bg);
    draw_hand(gfx, _second, state.second_angle);
    if (draw_hub_cap && _has_hub) {
        draw_hub(gfx);
    }
}

void AssetFaceRuntime::release_underlay() {
    if (_underlay) {
        free(_underlay);
        _underlay = nullptr;
    }
    _underlay_valid = false;
}

void AssetFaceRuntime::unload() {
    if (_dial) {
        free(_dial);
        _dial = nullptr;
    }
    _underlay_valid = false;
    _underlay_hour_angle = -999.0f;
    _underlay_minute_angle = -999.0f;
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
    _asset_index = -1;
    _has_second = false;
    _has_hub = false;
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
    release_underlay();
    if (!meta || !meta->blob) {
        return false;
    }
    _asset_index = asset_index;

    uint32_t magic = 0;
    memcpy_P(&magic, meta->blob, 4);
    if (magic != kMagic) {
        Serial.println("[ASSET] bad magic");
        return false;
    }

    _pivot_x = meta->pivot_x;
    _pivot_y = meta->pivot_y;
    _has_second = meta->has_second();
    _has_hub = meta->has_hub();
    _subdial_count = meta->subdial_count;

    _dial = (uint16_t*)malloc(kDialBytes);
    if (!_dial) {
        Serial.println("[ASSET] dial alloc failed");
        return false;
    }
    _underlay_valid = false;
    memcpy_P(_dial, meta->blob + kPackHeaderSize, kDialBytes);
    for (size_t i = 0; i < 240u * 240u; ++i) {
        const uint16_t c = _dial[i];
        _dial[i] = (uint16_t)((c >> 8) | (c << 8));
    }

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

    if (!ensure_underlay()) {
        Serial.printf("[ASSET] loaded %s heap=%u (no underlay; smooth may be limited)\n", meta->id,
                      (unsigned)ESP.getFreeHeap());
    } else {
        Serial.printf("[ASSET] loaded %s heap=%u underlay=yes\n", meta->id, (unsigned)ESP.getFreeHeap());
    }
    return true;
}

void AssetFaceRuntime::draw_hand(lgfx::LovyanGFX& gfx, const HandBuffer& hand, float angle_deg) const {
    draw_hand_at(gfx, hand, _pivot_x, _pivot_y, angle_deg);
}

void AssetFaceRuntime::draw_hand_at(lgfx::LovyanGFX& gfx, const HandBuffer& hand, int pivot_x, int pivot_y,
                                    float angle_deg) const {
    if (!hand.active || !hand.pixels) {
        return;
    }
    // LovyanGFX: 0° = sprite upright (12 o'clock); matches angles_from_time (0 = 12, 90 = 3).
    const float angle = angle_deg + hand.offset_deg;
    gfx.pushImageRotateZoomWithAA(pivot_x, pivot_y, hand.pivot_x, hand.pivot_y, angle, 1.0f, 1.0f, hand.width,
                                  hand.height, hand.pixels, 0u);
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

static bool mask_has(ClockHandMask mask, ClockHandMask bit) {
    return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(bit)) != 0;
}

void AssetFaceRuntime::draw(lgfx::LGFX_Device& gfx, const AnalogClockState& state, ClockHandMask mask,
                            const AnalogClockState* prev_state) {
    if (!_dial) {
        gfx.fillScreen(TFT_BLACK);
        gfx.setTextColor(TFT_WHITE);
        gfx.drawString("No face", 120, 120);
        return;
    }

    const bool full = mask_has(mask, ClockHandMask::Dial) || mask == ClockHandMask::All;
    const bool second_only =
        !full && mask_has(mask, ClockHandMask::Second) && !mask_has(mask, ClockHandMask::Hour) &&
        !mask_has(mask, ClockHandMask::Minute) && !mask_has(mask, ClockHandMask::ChronoSubdials);
    const bool chrono_only =
        !full && mask_has(mask, ClockHandMask::ChronoSubdials) && !any_hand(mask);

    if (full && _dial && !_underlay) {
        (void)ensure_underlay();
    }
    const bool underlay_stale =
        !_underlay_valid || fabsf(state.hour_angle - _underlay_hour_angle) > 0.05f ||
        fabsf(state.minute_angle - _underlay_minute_angle) > 0.05f;
    if (_underlay && (full || underlay_stale)) {
        rebuild_underlay(state);
    }
    const bool use_underlay = _underlay_valid && _underlay;

    if (full) {
        if (use_underlay) {
            gfx.pushImage(0, 0, 240, 240, _underlay);
        } else {
            gfx.pushImage(0, 0, 240, 240, _dial);
            draw_center_wall_hands(gfx, state, true, true, _has_second);
        }
    } else if (second_only && prev_state && _has_second) {
        draw_second_sweep(gfx, state, *prev_state, mask_has(mask, ClockHandMask::Hub));
    } else if (!chrono_only && prev_state) {
        if (use_underlay) {
            if (mask_has(mask, ClockHandMask::Hour) || mask_has(mask, ClockHandMask::Minute)) {
                patch_underlay_hands(gfx, state, *prev_state, mask);
            }
            if (mask_has(mask, ClockHandMask::Second) && _has_second) {
                draw_hand(gfx, _second, state.second_angle);
            }
        } else {
            gfx.pushImage(0, 0, 240, 240, _dial);
            draw_center_wall_hands(gfx, state, true, true, _has_second);
        }
    } else if (!chrono_only) {
        gfx.pushImage(0, 0, 240, 240, use_underlay ? _underlay : _dial);
        if (!use_underlay) {
            draw_center_wall_hands(gfx, state, true, true, _has_second);
        } else if (_has_second) {
            draw_hand(gfx, _second, state.second_angle);
        }
    }

    if (chrono_only && state.draw_chrono) {
        for (uint8_t i = 0; i < _subdial_count; ++i) {
            restore_subdial_patch(gfx, _subdials[i]);
        }
    }

    if (_has_second && use_underlay && full) {
        draw_hand(gfx, _second, state.second_angle);
    }
    if (state.draw_chrono && (full || mask_has(mask, ClockHandMask::ChronoSubdials))) {
        for (uint8_t i = 0; i < _subdial_count; ++i) {
            const LoadedSubdial& sd = _subdials[i];
            if (!sd.hand.active) {
                continue;
            }
            const float angle = subdial_angle(sd.role, state);
            draw_hand_at(gfx, sd.hand, sd.x, sd.y, angle);
        }
    }
    const bool show_hub = _has_hub && !second_only &&
                          (full || mask_has(mask, ClockHandMask::Hub) ||
                           mask_has(mask, ClockHandMask::Minute) ||
                           mask_has(mask, ClockHandMask::Hour));
    if (show_hub) {
        draw_hub(gfx);
    }
}

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

} // namespace

void AssetFaceRuntime::unload() {
    if (_dial) {
        free(_dial);
        _dial = nullptr;
    }
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

#if DEBUG_DISPLAY
    Serial.printf("[ASSET] loaded %s free heap=%u\n", meta->id, (unsigned)ESP.getFreeHeap());
#endif
    return true;
}

void AssetFaceRuntime::draw_hand(lgfx::LGFX_Device& gfx, const HandBuffer& hand, float angle_deg) const {
    draw_hand_at(gfx, hand, _pivot_x, _pivot_y, angle_deg);
}

void AssetFaceRuntime::draw_hand_at(lgfx::LGFX_Device& gfx, const HandBuffer& hand, int pivot_x, int pivot_y,
                                    float angle_deg) const {
    if (!hand.active || !hand.pixels) {
        return;
    }
    // LovyanGFX: 0° = sprite upright (12 o'clock); matches angles_from_time (0 = 12, 90 = 3).
    const float angle = angle_deg + hand.offset_deg;
    gfx.pushImageRotateZoomWithAA(pivot_x, pivot_y, hand.pivot_x, hand.pivot_y, angle, 1.0f, 1.0f, hand.width,
                                  hand.height, hand.pixels, 0u);
}

void AssetFaceRuntime::draw_hub(lgfx::LGFX_Device& gfx) const {
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

void AssetFaceRuntime::draw(lgfx::LGFX_Device& gfx, const AnalogClockState& state) {
    if (!_dial) {
        gfx.fillScreen(TFT_BLACK);
        gfx.setTextColor(TFT_WHITE);
        gfx.drawString("No face", 120, 120);
        return;
    }

    gfx.pushImage(0, 0, 240, 240, _dial);
    draw_hand(gfx, _hour, state.hour_angle);
    draw_hand(gfx, _minute, state.minute_angle);
    if (_has_second) {
        draw_hand(gfx, _second, state.second_angle);
    }
    if (state.draw_chrono) {
        for (uint8_t i = 0; i < _subdial_count; ++i) {
            const LoadedSubdial& sd = _subdials[i];
            if (!sd.hand.active) {
                continue;
            }
            const float angle = subdial_angle(sd.role, state);
            draw_hand_at(gfx, sd.hand, sd.x, sd.y, angle);
        }
    }
    draw_hub(gfx);
}

#include "clock/procedural_compositor.h"

#include "clock/clock_digital.h"
#include "clock/clock_draw.h"

#include <cstring>

namespace {

constexpr int kScreenW = 240;
constexpr int kScreenH = 240;
constexpr size_t kDialBytes = (size_t)kScreenW * kScreenH * 2;
constexpr int kCenterX = clock_draw::kCenterX;
constexpr int kCenterY = clock_draw::kCenterY;
constexpr int kHandPad = 7;

/** Band bounds for Classic Digital–style overlay (must match face readout layout). */
clock_digital::DigitalReadoutStyle digital_band_style() {
    clock_digital::DigitalReadoutStyle style;
    style.time_text_size = 2;
    style.date_text_size = 1;
    style.time_y = 52;
    style.date_y = 188;
    return style;
}

constexpr int kMaxBandPieces = 8;

using BandRect = ProceduralCompositorRect;

void split_rect_exclude(BandRect r, BandRect cut, BandRect* out, int& out_count, int out_max) {
    out_count = 0;
    if (r.empty()) {
        return;
    }
    if (r.x >= cut.x + cut.w || cut.x >= r.x + r.w || r.y >= cut.y + cut.h || cut.y >= r.y + r.h) {
        if (out_max > 0) {
            out[0] = r;
            out_count = 1;
        }
        return;
    }

    const int ex0 = (cut.x > r.x) ? cut.x : r.x;
    const int ey0 = (cut.y > r.y) ? cut.y : r.y;
    const int ex1 = (cut.x + cut.w < r.x + r.w) ? cut.x + cut.w : r.x + r.w;
    const int ey1 = (cut.y + cut.h < r.y + r.h) ? cut.y + cut.h : r.y + r.h;

    auto push = [&](int x, int y, int w, int h) {
        if (out_count >= out_max || w <= 0 || h <= 0) {
            return;
        }
        out[out_count++] = BandRect{x, y, w, h};
    };

    if (ey0 > r.y) {
        push(r.x, r.y, r.w, ey0 - r.y);
    }
    if (ey1 < r.y + r.h) {
        push(r.x, ey1, r.w, r.y + r.h - ey1);
    }
    const int mid_h = ey1 - ey0;
    if (mid_h > 0) {
        if (ex0 > r.x) {
            push(r.x, ey0, ex0 - r.x, mid_h);
        }
        if (ex1 < r.x + r.w) {
            push(ex1, ey0, r.x + r.w - ex1, mid_h);
        }
    }
}

int band_pieces_excluding(BandRect band, BandRect exclude, BandRect* out, int out_max) {
    if (band.empty()) {
        return 0;
    }
    if (exclude.empty()) {
        if (out_max > 0) {
            out[0] = band;
        }
        return 1;
    }

    BandRect pieces[kMaxBandPieces];
    int piece_n = 0;
    split_rect_exclude(band, exclude, pieces, piece_n, kMaxBandPieces);
    const int n = (piece_n < out_max) ? piece_n : out_max;
    for (int i = 0; i < n; ++i) {
        out[i] = pieces[i];
    }
    return n;
}

} // namespace

void ProceduralCompositor::reset() {
    if (_static) {
        free(_static);
        _static = nullptr;
    }
    if (_patch) {
        free(_patch);
        _patch = nullptr;
        _patch_pixels = 0;
    }
    _static_valid = false;
    _has_shown_second = false;
    _shown_second_angle = 0.0f;
}

bool ProceduralCompositor::ensure_static() {
    if (_static) {
        return true;
    }
    _static = (uint16_t*)malloc(kDialBytes);
    if (!_static) {
        Serial.println("[CLOCK] procedural static layer unavailable");
        return false;
    }
    _static_valid = false;
    return true;
}

bool ProceduralCompositor::ensure_patch(size_t pixel_count) {
    if (_patch && _patch_pixels >= pixel_count) {
        return true;
    }
    if (_patch) {
        free(_patch);
        _patch = nullptr;
        _patch_pixels = 0;
    }
    _patch = (uint16_t*)malloc(pixel_count * sizeof(uint16_t));
    if (!_patch) {
        return false;
    }
    _patch_pixels = pixel_count;
    return true;
}

bool ProceduralCompositor::rebuild_static(const ClockFace* face, const AnalogClockState& state) {
    if (!face || !ensure_static()) {
        _static_valid = false;
        return false;
    }

    lgfx::LGFX_Sprite layer;
    layer.setColorDepth(lgfx::color_depth_t::rgb565_2Byte);
    layer.setBuffer(_static, kScreenW, kScreenH, lgfx::color_depth_t::rgb565_2Byte);

    face->draw_background(layer);
    face->draw_static(layer, state.wall);
    if (face->draw_digital_overlay) {
        face->draw_digital_overlay(layer, state.wall);
    }
    face->draw_hands(layer, state, false, ClockHandMask::Hour | ClockHandMask::Minute | ClockHandMask::Hub,
                     kCenterX, kCenterY);

    _static_valid = true;
    return true;
}

ProceduralCompositor::Rect ProceduralCompositor::clip(Rect r) {
    if (r.empty()) {
        return Rect{};
    }
    if (r.x < 0) {
        r.w += r.x;
        r.x = 0;
    }
    if (r.y < 0) {
        r.h += r.y;
        r.y = 0;
    }
    if (r.x + r.w > kScreenW) {
        r.w = kScreenW - r.x;
    }
    if (r.y + r.h > kScreenH) {
        r.h = kScreenH - r.y;
    }
    return r.empty() ? Rect{} : r;
}

ProceduralCompositor::Rect ProceduralCompositor::merge(Rect a, Rect b) {
    if (a.empty()) {
        return b;
    }
    if (b.empty()) {
        return a;
    }
    const int x0 = (a.x < b.x) ? a.x : b.x;
    const int y0 = (a.y < b.y) ? a.y : b.y;
    const int x1 = ((a.x + a.w) > (b.x + b.w)) ? (a.x + a.w) : (b.x + b.w);
    const int y1 = ((a.y + a.h) > (b.y + b.h)) ? (a.y + a.h) : (b.y + b.h);
    return Rect{x0, y0, x1 - x0, y1 - y0};
}

bool ProceduralCompositor::intersects(const Rect& a, const Rect& b) {
    if (a.empty() || b.empty()) {
        return false;
    }
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

ProceduralCompositor::Rect ProceduralCompositor::intersection(Rect a, Rect b) {
    if (!intersects(a, b)) {
        return Rect{};
    }
    const int x0 = (a.x > b.x) ? a.x : b.x;
    const int y0 = (a.y > b.y) ? a.y : b.y;
    const int x1 = (a.x + a.w < b.x + b.w) ? a.x + a.w : b.x + b.w;
    const int y1 = (a.y + a.h < b.y + b.h) ? a.y + a.h : b.y + b.h;
    return Rect{x0, y0, x1 - x0, y1 - y0};
}

void ProceduralCompositor::copy_static_rect_to_patch(Rect patch_origin, Rect src_on_screen) {
    src_on_screen = clip(src_on_screen);
    const Rect isect = intersection(patch_origin, src_on_screen);
    if (isect.empty() || !_static || !_patch) {
        return;
    }
    for (int row = 0; row < isect.h; ++row) {
        const int screen_y = isect.y + row;
        const int patch_y = screen_y - patch_origin.y;
        const size_t src_off = (size_t)screen_y * (size_t)kScreenW + (size_t)isect.x;
        const size_t dst_off = (size_t)patch_y * (size_t)patch_origin.w + (size_t)(isect.x - patch_origin.x);
        memcpy(&_patch[dst_off], &_static[src_off], (size_t)isect.w * sizeof(uint16_t));
    }
}

ProceduralCompositor::Rect ProceduralCompositor::second_hand_rect(const ClockFace* face,
                                                                   float angle_deg) const {
    int tail_x = 0, tail_y = 0, tip_x = 0, tip_y = 0;
    clock_draw::polar_to_xy(kCenterX, kCenterY, angle_deg + 180.0f, face->second_tail_len, tail_x, tail_y);
    clock_draw::polar_to_xy(kCenterX, kCenterY, angle_deg, face->second_tip_len, tip_x, tip_y);

    int min_x = tail_x, min_y = tail_y, max_x = tail_x, max_y = tail_y;
    const int pts[2][2] = {{tip_x, tip_y}, {kCenterX, kCenterY}};
    for (const auto& p : pts) {
        if (p[0] < min_x) min_x = p[0];
        if (p[1] < min_y) min_y = p[1];
        if (p[0] > max_x) max_x = p[0];
        if (p[1] > max_y) max_y = p[1];
    }
    return Rect{min_x - kHandPad, min_y - kHandPad, (max_x - min_x) + 2 * kHandPad + 1,
                (max_y - min_y) + 2 * kHandPad + 1};
}

bool ProceduralCompositor::push_band_patch(lgfx::LGFX_Device& gfx, Rect band) {
    band = clip(band);
    if (band.empty() || !_static || !_static_valid) {
        return false;
    }
    const size_t pixels = (size_t)band.w * (size_t)band.h;
    if (!ensure_patch(pixels)) {
        return false;
    }

    copy_static_rect_to_patch(band, band);
    gfx.pushImage(band.x, band.y, band.w, band.h, _patch);
    return true;
}

bool ProceduralCompositor::push_hand_composite_patch(lgfx::LGFX_Device& gfx, const ClockFace* face,
                                                       const AnalogClockState& state, Rect hand) {
    hand = clip(hand);
    if (hand.empty() || !_static || !_static_valid || !face) {
        return false;
    }
    const size_t pixels = (size_t)hand.w * (size_t)hand.h;
    if (!ensure_patch(pixels)) {
        return false;
    }

    copy_static_rect_to_patch(hand, hand);

    lgfx::LGFX_Sprite layer;
    layer.setColorDepth(lgfx::color_depth_t::rgb565_2Byte);
    layer.setBuffer(_patch, hand.w, hand.h, lgfx::color_depth_t::rgb565_2Byte);
    face->draw_hands(layer, state, false, ClockHandMask::Second | ClockHandMask::Hub, kCenterX - hand.x,
                     kCenterY - hand.y);

    gfx.pushImage(hand.x, hand.y, hand.w, hand.h, _patch);
    return true;
}

void ProceduralCompositor::push_full(lgfx::LGFX_Device& gfx, const ClockFace* face,
                                     const AnalogClockState& state) {
    gfx.pushImage(0, 0, kScreenW, kScreenH, _static);
    push_hand_composite_patch(gfx, face, state, second_hand_rect(face, state.second_angle));
}

void ProceduralCompositor::remember_shown(const AnalogClockState& state) {
    _has_shown_second = true;
    _shown_second_angle = state.second_angle;
}

void ProceduralCompositor::present(lgfx::LGFX_Device& gfx, const ClockFace* face,
                                   const AnalogClockState& state, bool force_full) {
    if (!face) {
        return;
    }

    if (!ensure_static() || !rebuild_static(face, state)) {
        face->draw_background(gfx);
        face->draw_static(gfx, state.wall);
        if (face->draw_digital_overlay) {
            face->draw_digital_overlay(gfx, state.wall);
        }
        face->draw_hands(gfx, state, false, ClockHandMask::WallHands, kCenterX, kCenterY);
        _has_shown_second = false;
        return;
    }

    const Rect curr = second_hand_rect(face, state.second_angle);
    Rect hand_union = curr;
    if (_has_shown_second && !force_full) {
        hand_union = merge(second_hand_rect(face, _shown_second_angle), curr);
    }
    hand_union = clip(hand_union);

    const bool has_digital = face->draw_digital_overlay != nullptr;
    Rect time_band{};
    Rect date_band{};
    if (has_digital) {
        const auto style = digital_band_style();
        clock_digital::time_readout_rect(style, time_band.x, time_band.y, time_band.w, time_band.h);
        clock_digital::date_readout_rect(style, date_band.x, date_band.y, date_band.w, date_band.h);
    }

    if (force_full || !_has_shown_second) {
        push_full(gfx, face, state);
        remember_shown(state);
        return;
    }

    if (has_digital) {
        Rect pieces[kMaxBandPieces];
        int n = band_pieces_excluding(time_band, hand_union, pieces, kMaxBandPieces);
        for (int i = 0; i < n; ++i) {
            push_band_patch(gfx, clip(pieces[i]));
        }
        n = band_pieces_excluding(date_band, hand_union, pieces, kMaxBandPieces);
        for (int i = 0; i < n; ++i) {
            push_band_patch(gfx, clip(pieces[i]));
        }
    }

    if (!hand_union.empty()) {
        push_hand_composite_patch(gfx, face, state, hand_union);
    }

    remember_shown(state);
}

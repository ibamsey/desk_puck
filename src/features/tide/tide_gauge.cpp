#include "tide/tide_gauge.h"

#include "config.h"
#include "tide/tide_feed.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

constexpr int kCx = DISPLAY_WIDTH / 2;
constexpr int kCy = DISPLAY_HEIGHT / 2;

/** Single scale: midnight at top, one annulus for past and future. */
constexpr int kCentreR = 28;
constexpr int kPlotInner = 34;
constexpr int kPlotOuter = 92;
constexpr int kTickOuter = 97;
constexpr int kHourLabelR = 104;
constexpr int kBandFloor = 5;

constexpr float kWedgeDeg = 3.0f;
constexpr int kObsStepSec = 10 * 60;
constexpr uint8_t kMaxMarks = 16;
/** Fade to transparent shortly beyond the 2nd extreme in each direction. */
constexpr int kExtremeFadeSec = 40 * 60;
constexpr int kFallbackSpanSec = 6 * 3600;

constexpr uint16_t kBg = 0x0841;
constexpr uint16_t kTickColor = 0x6B4D;
constexpr uint16_t kLabelColor = 0xFFFF;
constexpr uint16_t kDimLabel = 0x9CF3;
constexpr uint16_t kObsColor = 0xFFE0;
constexpr uint16_t kNowColor = 0xFD20;
constexpr uint16_t kPredColor = 0x5DFF;

constexpr float kFieldPeakAlpha = 0.72f;
constexpr float kSlopeTilt = 0.24f;

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr Rgb kLwRgb = {20, 185, 100};
constexpr Rgb kMidRgb = {46, 60, 78};
constexpr Rgb kHwRgb = {40, 100, 245};

struct TideEvent {
    time_t epoch = 0;
    float height_m = 0.0f;
    bool is_high = false;
};

struct HeightScale {
    float min_m = 0.0f;
    float max_m = 4.0f;
};

struct TimeWindow {
    time_t past_start = 0;
    time_t past_fade_before = 0;
    time_t future_end = 0;
    time_t future_fade_after = 0;
    bool valid = false;
};

float clamp01(float v) {
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

float lerpf(float a, float b, float t) {
    return a + (b - a) * clamp01(t);
}

uint16_t rgb_to_565(const Rgb& c) {
    return (uint16_t)(((uint16_t)(c.r & 0xF8) << 8) | ((uint16_t)(c.g & 0xFC) << 3) | (c.b >> 3));
}

Rgb rgb_lerp(const Rgb& a, const Rgb& b, float t) {
    t = clamp01(t);
    Rgb out;
    out.r = (uint8_t)lroundf(a.r + (b.r - a.r) * t);
    out.g = (uint8_t)lroundf(a.g + (b.g - a.g) * t);
    out.b = (uint8_t)lroundf(a.b + (b.b - a.b) * t);
    return out;
}

Rgb rgb_scale(const Rgb& c, float k) {
    Rgb out;
    out.r = (uint8_t)lroundf(clampf(c.r * k, 0.0f, 255.0f));
    out.g = (uint8_t)lroundf(clampf(c.g * k, 0.0f, 255.0f));
    out.b = (uint8_t)lroundf(clampf(c.b * k, 0.0f, 255.0f));
    return out;
}

Rgb phase_rgb(float u) {
    if (u <= 0.5f) {
        return rgb_lerp(kLwRgb, kMidRgb, u * 2.0f);
    }
    return rgb_lerp(kMidRgb, kHwRgb, (u - 0.5f) * 2.0f);
}

uint16_t blend565(uint16_t fg, uint16_t bg, float a) {
    a = clamp01(a);
    const uint8_t fr = (fg >> 11) & 0x1F;
    const uint8_t fg_g = (fg >> 5) & 0x3F;
    const uint8_t fb = fg & 0x1F;
    const uint8_t br = (bg >> 11) & 0x1F;
    const uint8_t bg_g = (bg >> 5) & 0x3F;
    const uint8_t bb = bg & 0x1F;
    const uint8_t r = (uint8_t)lroundf((1.0f - a) * br + a * fr);
    const uint8_t g = (uint8_t)lroundf((1.0f - a) * bg_g + a * fg_g);
    const uint8_t b = (uint8_t)lroundf((1.0f - a) * bb + a * fb);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

float seconds_of_day_local(time_t epoch) {
    struct tm tm;
    localtime_r(&epoch, &tm);
    return (float)(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
}

float time_to_angle_deg(float sec_of_day) {
    while (sec_of_day >= 86400.0f) {
        sec_of_day -= 86400.0f;
    }
    while (sec_of_day < 0.0f) {
        sec_of_day += 86400.0f;
    }
    return 270.0f + (sec_of_day / 86400.0f) * 360.0f;
}

void polar_xy(float angle_deg, float radius, int& x, int& y) {
    const float rad = angle_deg * (float)M_PI / 180.0f;
    x = kCx + (int)lroundf(cosf(rad) * radius);
    y = kCy + (int)lroundf(sinf(rad) * radius);
}

void scan_height_scale(const TideFeedSnapshot& snap, HeightScale& scale) {
    scale.min_m = 999.0f;
    scale.max_m = -999.0f;
    auto scan = [&](const TideSeries& series) {
        for (uint16_t i = 0; i < series.count; i++) {
            const float h = series.points[i].height_m;
            if (h < scale.min_m) {
                scale.min_m = h;
            }
            if (h > scale.max_m) {
                scale.max_m = h;
            }
        }
    };
    scan(snap.observed);
    scan(snap.predicted);
    if (scale.max_m <= scale.min_m) {
        scale.min_m = 0.0f;
        scale.max_m = 4.0f;
        return;
    }
    const float pad = 0.15f;
    scale.min_m -= pad;
    scale.max_m += pad;
}

float height_norm(float h, const HeightScale& scale) {
    const float span = scale.max_m - scale.min_m;
    if (span <= 0.01f) {
        return 0.5f;
    }
    return clamp01((h - scale.min_m) / span);
}

float band_radius(float u) {
    const float lo = (float)(kPlotInner + kBandFloor);
    return lo + clamp01(u) * ((float)kPlotOuter - lo);
}

float series_height_at(const TideSeries& series, time_t epoch) {
    if (series.count == 0) {
        return NAN;
    }
    if (epoch <= series.points[0].epoch_utc) {
        return series.points[0].height_m;
    }
    const TideSample& last = series.points[series.count - 1];
    if (epoch >= last.epoch_utc) {
        return last.height_m;
    }
    for (uint16_t i = 1; i < series.count; i++) {
        const TideSample& a = series.points[i - 1];
        const TideSample& b = series.points[i];
        if (epoch <= b.epoch_utc) {
            const float dt = (float)(b.epoch_utc - a.epoch_utc);
            if (dt <= 0.0f) {
                return b.height_m;
            }
            const float f = (float)(epoch - a.epoch_utc) / dt;
            return a.height_m + f * (b.height_m - a.height_m);
        }
    }
    return last.height_m;
}

float slope_norm_at(const TideSeries& series, time_t epoch, const HeightScale& scale) {
    const float h0 = series_height_at(series, epoch - 600);
    const float h1 = series_height_at(series, epoch + 600);
    if (isnan(h0) || isnan(h1)) {
        return 0.0f;
    }
    const float span = scale.max_m - scale.min_m;
    if (span <= 0.01f) {
        return 0.0f;
    }
    const float slope = (h1 - h0) / 1200.0f;
    const float ref = span / (3.0f * 3600.0f);
    return clampf(slope / ref, -1.0f, 1.0f);
}

uint16_t field_color(float u, float slope_norm, float alpha) {
    const Rgb lit = rgb_scale(phase_rgb(u), 1.0f + kSlopeTilt * slope_norm);
    return blend565(rgb_to_565(lit), kBg, kFieldPeakAlpha * alpha);
}

void fill_wedge(lgfx::LGFX_Device& gfx, int r_out, int r_in, float a0, float a1, uint16_t color) {
    while (a0 >= 360.0f) {
        a0 -= 360.0f;
        a1 -= 360.0f;
    }
    if (a1 <= 360.0f) {
        gfx.fillArc(kCx, kCy, r_out, r_in, a0, a1, color);
        return;
    }
    gfx.fillArc(kCx, kCy, r_out, r_in, a0, 360.0f, color);
    gfx.fillArc(kCx, kCy, r_out, r_in, 0.0f, a1 - 360.0f, color);
}

bool is_local_max(const TideSeries& series, uint16_t i) {
    if (i == 0 || i + 1 >= series.count) {
        return false;
    }
    const float cur = series.points[i].height_m;
    const float prev = series.points[i - 1].height_m;
    const float next = series.points[i + 1].height_m;
    return cur >= prev && cur >= next && (cur > prev + 0.008f || cur > next + 0.008f);
}

bool is_local_min(const TideSeries& series, uint16_t i) {
    if (i == 0 || i + 1 >= series.count) {
        return false;
    }
    const float cur = series.points[i].height_m;
    const float prev = series.points[i - 1].height_m;
    const float next = series.points[i + 1].height_m;
    return cur <= prev && cur <= next && (prev > cur + 0.008f || next > cur + 0.008f);
}

int compare_events(const void* a, const void* b) {
    const TideEvent* ea = static_cast<const TideEvent*>(a);
    const TideEvent* eb = static_cast<const TideEvent*>(b);
    if (ea->epoch < eb->epoch) {
        return -1;
    }
    if (ea->epoch > eb->epoch) {
        return 1;
    }
    return 0;
}

void dedupe_marks(TideEvent* events, uint8_t& n) {
    if (n <= 1) {
        return;
    }
    qsort(events, n, sizeof(TideEvent), compare_events);
    uint8_t w = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (w == 0) {
            events[w++] = events[i];
            continue;
        }
        TideEvent& prev = events[w - 1];
        const TideEvent& cur = events[i];
        if (cur.is_high != prev.is_high) {
            events[w++] = cur;
        } else if (cur.is_high ? (cur.height_m > prev.height_m) : (cur.height_m < prev.height_m)) {
            prev = cur;
        }
    }
    n = w;
}

uint8_t collect_marks(const TideFeedSnapshot& snap, TideEvent* out, uint8_t max_out) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < snap.hwlw_count && n < max_out; i++) {
        const TideHwLwEvent& e = snap.hwlw[i];
        out[n].epoch = e.epoch;
        out[n].height_m = e.height_m;
        out[n].is_high = e.is_high;
        n++;
    }
    if (n >= 2) {
        dedupe_marks(out, n);
        return n;
    }

    n = 0;
    for (uint16_t i = 1; i + 1 < snap.predicted.count && n < max_out; i++) {
        const TideSample& p = snap.predicted.points[i];
        const bool hi = is_local_max(snap.predicted, i);
        if (!hi && !is_local_min(snap.predicted, i)) {
            continue;
        }
        out[n].epoch = p.epoch_utc;
        out[n].height_m = p.height_m;
        out[n].is_high = hi;
        n++;
    }
    dedupe_marks(out, n);
    return n;
}

TimeWindow windows_from_extremes(const TideEvent* events, uint8_t n, time_t now) {
    TimeWindow w;
    if (n == 0) {
        w.past_start = now - kFallbackSpanSec;
        w.past_fade_before = w.past_start - kExtremeFadeSec;
        w.future_end = now + kFallbackSpanSec;
        w.future_fade_after = w.future_end + kExtremeFadeSec;
        w.valid = true;
        return w;
    }

    uint8_t past_n = 0;
    uint8_t future_n = 0;
    time_t past_epochs[8];
    time_t future_epochs[8];

    for (uint8_t i = 0; i < n; i++) {
        if (events[i].epoch <= now) {
            if (past_n < 8) {
                past_epochs[past_n++] = events[i].epoch;
            }
        } else if (future_n < 8) {
            future_epochs[future_n++] = events[i].epoch;
        }
    }

    if (past_n >= 2) {
        w.past_start = past_epochs[past_n - 2];
    } else if (past_n == 1) {
        w.past_start = past_epochs[0];
    } else {
        w.past_start = now - kFallbackSpanSec;
    }
    w.past_fade_before = w.past_start - kExtremeFadeSec;

    if (future_n >= 2) {
        w.future_end = future_epochs[1];
    } else if (future_n == 1) {
        w.future_end = future_epochs[0];
    } else {
        w.future_end = now + kFallbackSpanSec;
    }
    w.future_fade_after = w.future_end + kExtremeFadeSec;

    w.valid = true;
    return w;
}

float past_alpha(time_t t, const TimeWindow& w, time_t now) {
    if (t > now || t < w.past_fade_before) {
        return 0.0f;
    }
    if (t >= w.past_start) {
        return 1.0f;
    }
    return clamp01((float)(t - w.past_fade_before) / (float)kExtremeFadeSec);
}

float future_alpha(time_t t, const TimeWindow& w, time_t now) {
    if (t < now || t > w.future_fade_after) {
        return 0.0f;
    }
    if (t <= w.future_end) {
        return 1.0f;
    }
    return clamp01((float)(w.future_fade_after - t) / (float)kExtremeFadeSec);
}

void draw_field_span(lgfx::LGFX_Device& gfx, const TideSeries& series, time_t now, const TimeWindow& win,
                     const HeightScale& scale, bool future) {
    const time_t t_lo = future ? now : win.past_fade_before;
    const time_t t_hi = future ? win.future_fade_after : now;
    if (t_hi <= t_lo) {
        return;
    }

    const float span_sec = (float)(t_hi - t_lo);
    const float total_deg = 360.0f * span_sec / 86400.0f;
    const int steps = (int)lroundf(total_deg / kWedgeDeg);
    if (steps < 1) {
        return;
    }
    const float step_sec = span_sec / (float)steps;

    for (int i = 0; i < steps; i++) {
        const time_t t0 = future ? t_lo + (time_t)lroundf((float)i * step_sec)
                                 : t_lo + (time_t)lroundf((float)i * step_sec);
        const time_t t_mid = t0 + (time_t)lroundf(step_sec * 0.5f);
        if (future && t_mid < now) {
            continue;
        }
        if (!future && t_mid > now) {
            continue;
        }

        const float alpha = future ? future_alpha(t_mid, win, now) : past_alpha(t_mid, win, now);
        if (alpha < 0.03f) {
            continue;
        }

        const float h = series_height_at(series, t_mid);
        if (isnan(h)) {
            continue;
        }
        const float u = height_norm(h, scale);
        const uint16_t color = field_color(u, slope_norm_at(series, t_mid, scale), alpha);
        const float a0 = time_to_angle_deg(seconds_of_day_local(t0));
        fill_wedge(gfx, (int)lroundf(band_radius(u)), kPlotInner, a0, a0 + kWedgeDeg, color);
    }
}

/** Past continuum on the same scale (drawn first; prediction draws over it). */
void draw_past_field(lgfx::LGFX_Device& gfx, const TideFeedSnapshot& snap, time_t now,
                     const TimeWindow& win, const HeightScale& scale) {
    draw_field_span(gfx, snap.predicted, now, win, scale, false);
}

void draw_future_field(lgfx::LGFX_Device& gfx, const TideFeedSnapshot& snap, time_t now,
                       const TimeWindow& win, const HeightScale& scale) {
    draw_field_span(gfx, snap.predicted, now, win, scale, true);
}

void draw_observed_continuum(lgfx::LGFX_Device& gfx, const TideSeries& obs, time_t now,
                             const TimeWindow& win, const HeightScale& scale) {
    if (obs.count == 0) {
        return;
    }

    int prev_x = 0;
    int prev_y = 0;
    bool have_prev = false;
    const time_t t_start = win.past_fade_before;
    const int step = kObsStepSec;

    for (time_t t = now; t >= t_start; t -= step) {
        const float alpha = past_alpha(t, win, now);
        if (alpha < 0.05f) {
            break;
        }
        const float h = series_height_at(obs, t);
        if (isnan(h) || t < obs.points[0].epoch_utc) {
            have_prev = false;
            continue;
        }
        const float ang = time_to_angle_deg(seconds_of_day_local(t));
        int x = 0;
        int y = 0;
        polar_xy(ang, band_radius(height_norm(h, scale)), x, y);
        if (have_prev) {
            gfx.drawLine(prev_x, prev_y, x, y, blend565(kObsColor, kBg, alpha * 0.85f));
        }
        prev_x = x;
        prev_y = y;
        have_prev = true;
    }
}

void draw_marks(lgfx::LGFX_Device& gfx, const TideEvent* events, uint8_t n, time_t now,
                const TimeWindow& win, const HeightScale& scale) {
    for (uint8_t i = 0; i < n; i++) {
        const TideEvent& e = events[i];
        const bool future = e.epoch > now;
        float alpha = future ? future_alpha(e.epoch, win, now) : past_alpha(e.epoch, win, now);
        if (e.epoch == now) {
            alpha = 1.0f;
        }
        if (alpha < 0.08f) {
            continue;
        }

        const float u = height_norm(e.height_m, scale);
        const float r = band_radius(u);
        const float ang = time_to_angle_deg(seconds_of_day_local(e.epoch));

        int xi = 0;
        int yi = 0;
        int xo = 0;
        int yo = 0;
        polar_xy(ang, (float)kPlotInner, xi, yi);
        polar_xy(ang, r, xo, yo);
        gfx.drawLine(xi, yi, xo, yo, blend565(kLabelColor, kBg, alpha * 0.5f));

        const uint16_t dot = blend565(rgb_to_565(phase_rgb(u)), kBg, alpha * 0.9f);
        gfx.fillCircle(xo, yo, future ? 3 : 2, dot);
        if (future) {
            gfx.drawCircle(xo, yo, 3, blend565(kLabelColor, kBg, alpha * 0.75f));
        }
    }
}

void draw_hour_ring(lgfx::LGFX_Device& gfx) {
    gfx.drawCircle(kCx, kCy, kPlotOuter, kTickColor);
    gfx.setTextDatum(textdatum_t::middle_center);
    gfx.setTextSize(1);
    gfx.setTextColor(kDimLabel, kBg);

    for (int h = 0; h < 24; h += 3) {
        const float ang = time_to_angle_deg((float)(h * 3600));
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        polar_xy(ang, (float)kPlotOuter, x0, y0);
        polar_xy(ang, (float)kTickOuter, x1, y1);
        gfx.drawLine(x0, y0, x1, y1, kTickColor);

        char label[4];
        snprintf(label, sizeof(label), "%d", h);
        int tx = 0;
        int ty = 0;
        polar_xy(ang, (float)kHourLabelR, tx, ty);
        gfx.drawString(label, tx, ty);
    }
}

void draw_now_axis(lgfx::LGFX_Device& gfx, const TideFeedSnapshot& snap, time_t now,
                   const HeightScale& scale) {
    const float ang = time_to_angle_deg(seconds_of_day_local(now));
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    polar_xy(ang, (float)(kCentreR + 2), x0, y0);
    polar_xy(ang, (float)kTickOuter, x1, y1);
    gfx.drawLine(x0, y0, x1, y1, blend565(kNowColor, kBg, 0.75f));

    const float pred_h = series_height_at(snap.predicted, now);
    if (!isnan(pred_h)) {
        int px = 0;
        int py = 0;
        polar_xy(ang, band_radius(height_norm(pred_h, scale)), px, py);
        gfx.fillCircle(px, py, 2, kPredColor);
    }

    const float obs_h = series_height_at(snap.observed, now);
    if (!isnan(obs_h)) {
        int ox = 0;
        int oy = 0;
        polar_xy(ang, band_radius(height_norm(obs_h, scale)), ox, oy);
        gfx.fillCircle(ox, oy, 3, kObsColor);
        gfx.drawCircle(ox, oy, 3, kLabelColor);
    }
}

const TideEvent* next_mark(const TideEvent* events, uint8_t n, time_t now) {
    const TideEvent* best = nullptr;
    for (uint8_t i = 0; i < n; i++) {
        if (events[i].epoch <= now) {
            continue;
        }
        if (!best || events[i].epoch < best->epoch) {
            best = &events[i];
        }
    }
    return best;
}

void draw_centre_readout(lgfx::LGFX_Device& gfx, const TideFeedSnapshot& snap, const TideEvent* events,
                         uint8_t n, time_t now) {
    gfx.fillCircle(kCx, kCy, kCentreR, kBg);
    gfx.setTextDatum(textdatum_t::middle_center);
    gfx.setTextSize(1);

    const TideEvent* nx = next_mark(events, n, now);
    if (!nx) {
        gfx.setTextColor(kDimLabel, kBg);
        gfx.drawString("--", kCx, kCy);
        return;
    }

    const long secs = (long)(nx->epoch - now);
    const int hh = (int)(secs / 3600);
    const int mm = (int)((secs % 3600) / 60);

    char line[12];
    snprintf(line, sizeof(line), "%s %dh%02d", nx->is_high ? "HW" : "LW", hh, mm);
    gfx.setTextColor(rgb_to_565(phase_rgb(nx->is_high ? 1.0f : 0.0f)), kBg);
    gfx.drawString(line, kCx, kCy - 12);

    snprintf(line, sizeof(line), "%.2fm", (double)nx->height_m);
    gfx.setTextColor(kLabelColor, kBg);
    gfx.drawString(line, kCx, kCy);

    const float pred_h = series_height_at(snap.predicted, now);
    const float obs_h = series_height_at(snap.observed, now);
    if (!isnan(pred_h) && !isnan(obs_h) && snap.observed.count > 0) {
        snprintf(line, sizeof(line), "%+.2f", (double)(obs_h - pred_h));
        gfx.setTextColor(kObsColor, kBg);
        gfx.drawString(line, kCx, kCy + 12);
    }
}

} // namespace

void tide_polar_draw(lgfx::LGFX_Device& gfx, const TideFeedSnapshot& snap, const TidePolarContext& ctx) {
    gfx.fillScreen(kBg);

    HeightScale scale;
    scan_height_scale(snap, scale);

    TideEvent events[kMaxMarks];
    const uint8_t n = collect_marks(snap, events, kMaxMarks);
    const TimeWindow win = windows_from_extremes(events, n, ctx.now_epoch);

    // Z-order: past field, measured trace, then future prediction on top (same dial scale).
    draw_past_field(gfx, snap, ctx.now_epoch, win, scale);
    draw_observed_continuum(gfx, snap.observed, ctx.now_epoch, win, scale);
    draw_future_field(gfx, snap, ctx.now_epoch, win, scale);

    draw_hour_ring(gfx);
    draw_marks(gfx, events, n, ctx.now_epoch, win, scale);
    draw_now_axis(gfx, snap, ctx.now_epoch, scale);
    draw_centre_readout(gfx, snap, events, n, ctx.now_epoch);

    gfx.setTextDatum(textdatum_t::top_center);
    gfx.setTextColor(kDimLabel, kBg);
    gfx.setTextSize(1);
    gfx.drawString(TIDE_STATION_LABEL, kCx, 2);
}

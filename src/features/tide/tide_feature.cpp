#include "tide/tide_feature.h"

#include "config.h"
#include "clock/wall_time.h"
#include "tide/tide_gauge.h"
#include "tide/tide_feed.h"

#include <stdio.h>
#include <time.h>

void TideFeature::request_full() {
    _last_hour = _last_minute = _last_second = -1;
    _frac_anchor_second = -1;
    _frac_anchor_ms = 0;
    setDirty();
}

void TideFeature::onEnter() {
    Serial.println("[TIDE] enter");
    tide_feed_snapshot(_snap);
    _had_valid = _snap.valid;
    _face_yday = -1;
    _face_year = -1;
    if (!_snap.loading && !_snap.valid) {
        tide_feed_request_refresh();
    }
    request_full();
}

void TideFeature::onExit() {
    _frac_anchor_second = -1;
}

float TideFeature::second_fraction(unsigned long now_ms, int second) const {
    if (second != _frac_anchor_second) {
        return 0.0f;
    }
    if (_frac_anchor_ms == 0) {
        return 0.0f;
    }
    float frac = (float)(now_ms - _frac_anchor_ms) / 1000.0f;
    if (frac < 0.0f) {
        return 0.0f;
    }
    if (frac > 1.0f) {
        return 1.0f;
    }
    return frac;
}

void TideFeature::onTick(unsigned long now_ms) {
    // Static: a snapshot is ~4 KB and would dominate the loop task stack.
    static TideFeedSnapshot cur;
    tide_feed_snapshot(cur);
    const bool data_changed = cur.valid != _snap.valid || cur.loading != _snap.loading ||
                              strcmp(cur.error, _snap.error) != 0 ||
                              cur.observed.count != _snap.observed.count ||
                              cur.predicted.count != _snap.predicted.count;
    if (data_changed) {
        _snap = cur;
        if (_snap.valid && !_had_valid) {
            _had_valid = true;
        }
        request_full();
    } else {
        _snap = cur;
    }

    int h = 0;
    int m = 0;
    int s = 0;
    wall_time_now(h, m, s);

    if (s != _frac_anchor_second) {
        _frac_anchor_second = s;
        _frac_anchor_ms = now_ms;
    }

    // The window is anchored to now, so the whole face only shifts by 0.25 deg a minute.
    if ((h != _last_hour) || (m != _last_minute)) {
        _last_hour = h;
        _last_minute = m;
        _last_second = s;
        _smooth_last_draw_ms = now_ms;
        setDirty();
    }
}

bool TideFeature::onInput(InputEvent event) {
    if (event == InputEvent::Tap || event == InputEvent::DoubleTap) {
        tide_feed_request_refresh();
        request_full();
        return true;
    }
    return false;
}

void TideFeature::onDraw(lgfx::LGFX_Device& gfx) {
    const int cx = DISPLAY_WIDTH / 2;

    if (_snap.loading && !_snap.valid) {
        gfx.fillScreen(TFT_BLACK);
        gfx.setTextDatum(textdatum_t::middle_center);
        gfx.setTextColor(TFT_WHITE, TFT_BLACK);
        gfx.drawString("Fetching CCO data...", cx, DISPLAY_HEIGHT / 2 - 8);
        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.drawString("tap = refresh", cx, DISPLAY_HEIGHT / 2 + 12);
        return;
    }

    if (!_snap.valid) {
        gfx.fillScreen(TFT_BLACK);
        gfx.setTextDatum(textdatum_t::middle_center);
        gfx.setTextColor(TFT_ORANGE, TFT_BLACK);
        const char* err = _snap.error[0] ? _snap.error : tide_feed_last_error();
        if (err && err[0]) {
            char line[56];
            snprintf(line, sizeof(line), "%s", err);
            gfx.drawString(line, cx, DISPLAY_HEIGHT / 2 - 14);
            gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
            gfx.drawString("WiFi + time sync needed", cx, DISPLAY_HEIGHT / 2 + 4);
        } else {
            gfx.drawString("Waiting for WiFi / time", cx, DISPLAY_HEIGHT / 2 - 8);
        }
        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.drawString("tap = retry", cx, DISPLAY_HEIGHT / 2 + 12);
        return;
    }

    time_t now_epoch = time(nullptr);
    if (now_epoch <= 1700000000) {
        gfx.fillScreen(TFT_BLACK);
        gfx.setTextDatum(textdatum_t::middle_center);
        gfx.setTextColor(TFT_ORANGE, TFT_BLACK);
        gfx.drawString("Waiting for time sync", cx, DISPLAY_HEIGHT / 2 - 8);
        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.drawString("SNTP after WiFi", cx, DISPLAY_HEIGHT / 2 + 12);
        return;
    }

    int h = 0;
    int m = 0;
    int s = 0;
    wall_time_now(h, m, s);

    TidePolarContext ctx;
    ctx.now_epoch = now_epoch;
    ctx.second_frac = second_fraction(millis(), s);

    tide_polar_draw(gfx, _snap, ctx);

    gfx.setTextDatum(textdatum_t::bottom_center);
    gfx.setTextColor(TFT_DARKGREY, 0x0841);
    char age[28];
    snprintf(age, sizeof(age), "%lum tap=refresh", (unsigned)(_snap.fetched_age_ms / 60000UL));
    gfx.drawString(age, cx, DISPLAY_HEIGHT - 2);
}

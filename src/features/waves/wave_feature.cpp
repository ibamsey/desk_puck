#include "waves/wave_feature.h"

#include "config.h"
#include "waves/wave_style.h"

void WaveFeature::onEnter() {
    Serial.println("[WAVE] enter");
    wave_feed_snapshot(_snap);
    _had_valid = _snap.valid;
    if (!_snap.loading && !_snap.valid) {
        wave_feed_request_refresh();
    }
    _anim_origin_ms = millis();
    (void)_compositor.ensure_buffer();
    Serial.printf("[WAVE] free heap: %u\n", (unsigned)ESP.getFreeHeap());
    setDirty();
}

void WaveFeature::onExit() {
    _compositor.reset();
}

void WaveFeature::onTick(unsigned long now_ms) {
    (void)now_ms;
    WaveFeedSnapshot cur;
    wave_feed_snapshot(cur);
    const bool data_changed = cur.valid != _snap.valid || cur.loading != _snap.loading ||
                              strcmp(cur.error, _snap.error) != 0 ||
                              cur.hs_m != _snap.hs_m || cur.tp_s != _snap.tp_s;
    _snap = cur;
    if (_snap.valid && !_had_valid) {
        _had_valid = true;
    }

    if (_snap.valid) {
        setDirty();
    } else if (data_changed || _snap.loading) {
        setDirty();
    }
}

bool WaveFeature::onInput(InputEvent event) {
    if (event == InputEvent::Tap || event == InputEvent::DoubleTap) {
        wave_feed_request_refresh();
        setDirty();
        return true;
    }
    return false;
}

void WaveFeature::onDraw(lgfx::LGFX_Device& gfx) {
    const int cx = DISPLAY_WIDTH / 2;
    const unsigned long now_ms = millis();

    if (_snap.loading && !_snap.valid) {
        gfx.fillScreen(wave_style::kSky);
        gfx.setTextDatum(textdatum_t::middle_center);
        gfx.setTextColor(TFT_WHITE, wave_style::kSky);
        gfx.drawString("Fetching wave data...", cx, DISPLAY_HEIGHT / 2 - 8);
        gfx.setTextColor(TFT_DARKGREY, wave_style::kSky);
        gfx.drawString("tap = refresh", cx, DISPLAY_HEIGHT / 2 + 12);
        return;
    }

    if (!_snap.valid) {
        gfx.fillScreen(wave_style::kSky);
        gfx.setTextDatum(textdatum_t::middle_center);
        gfx.setTextColor(TFT_ORANGE, wave_style::kSky);
        const char* err = _snap.error[0] ? _snap.error : wave_feed_last_error();
        if (err && err[0]) {
            gfx.drawString(err, cx, DISPLAY_HEIGHT / 2 - 8);
        } else {
            gfx.drawString("Waiting for WiFi", cx, DISPLAY_HEIGHT / 2 - 8);
        }
        gfx.setTextColor(TFT_DARKGREY, wave_style::kSky);
        gfx.drawString("tap = retry", cx, DISPLAY_HEIGHT / 2 + 12);
        return;
    }

    _compositor.present(gfx, _snap.hs_m, _snap.tp_s, _anim_origin_ms, now_ms);
}

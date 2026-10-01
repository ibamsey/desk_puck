#include "diary/diary_feature.h"

#include "config.h"
#include "diary/diary_qr.h"
#include "diary/diary_sync.h"
#include "diary/gcal_auth.h"
#include "time/wall_clock.h"
#include "wifi/wifi_station.h"

#include <math.h>

namespace {

constexpr int kDialCy = DISPLAY_HEIGHT / 2;
constexpr int kSafeRadius = 107;
constexpr int kDateHeaderY = 22;
constexpr int kListTop = 36;
constexpr int kListBottom = DISPLAY_HEIGHT - 16;
constexpr int kVisibleListHeight = kListBottom - kListTop;
constexpr int kRowHeight = 48;
constexpr float kEventTextScale = 1.7f; // 15% below previous 2× scale
constexpr float kMetaTextScale = 1.0f;
constexpr int kTimeColWidth = 54;
constexpr float kScrollImpulsePxS = 1100.f;
constexpr float kScrollFrictionPerSec = 4.2f;
constexpr float kScrollStopPxS = 12.f;
constexpr float kScrollMaxPxS = 3200.f;

static int chord_half_width_at_y(int y, int cx, int cy, int radius) {
    const int dy = y - cy;
    const int r2 = radius * radius;
    const int dy2 = dy * dy;
    if (dy2 >= r2) {
        return 0;
    }
    return (int)sqrtf((float)(r2 - dy2));
}

static void format_time(int minutes, char* buf, size_t len) {
    if (len < 6) {
        return;
    }
    minutes = (minutes % (24 * 60) + 24 * 60) % (24 * 60);
    snprintf(buf, len, "%02d:%02d", minutes / 60, minutes % 60);
}

static void format_agenda_date_header(int year, int month, int day, char* buf, size_t len) {
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char* dow[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    struct tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_isdst = -1;
    mktime(&tm);
    const char* w = (tm.tm_wday >= 0 && tm.tm_wday <= 6) ? dow[tm.tm_wday] : "---";
    const char* mon = (month >= 1 && month <= 12) ? months[month - 1] : "---";
    int cur_y = 0;
    int cur_m = 0;
    int cur_d = 0;
    wall_clock_now_ymd(cur_y, cur_m, cur_d);
    (void)cur_m;
    (void)cur_d;
    if (year == cur_y) {
        snprintf(buf, len, "%s · %s %d", w, mon, day);
    } else {
        snprintf(buf, len, "%s · %s %d, %d", w, mon, day, year);
    }
}

static int ymd_key(int y, int m, int d) {
    return y * 10000 + m * 100 + d;
}

} // namespace

int DiaryFeature::max_scroll_px() const {
    if (_content_height_px <= kVisibleListHeight) {
        return 0;
    }
    return _content_height_px - kVisibleListHeight;
}

void DiaryFeature::clamp_scroll_offset() {
    const int max_scroll = max_scroll_px();
    if (_scroll_offset_y < 0.f) {
        _scroll_offset_y = 0.f;
    } else if (_scroll_offset_y > (float)max_scroll) {
        _scroll_offset_y = (float)max_scroll;
    }
    if (_scroll_offset_y <= 0.f && _scroll_velocity_px_s < 0.f) {
        _scroll_velocity_px_s = 0.f;
    }
    if (_scroll_offset_y >= (float)max_scroll && _scroll_velocity_px_s > 0.f) {
        _scroll_velocity_px_s = 0.f;
    }
}

void DiaryFeature::refresh_content_height() {
    _content_height_px = (int)_buffer.count * kRowHeight;
    clamp_scroll_offset();
}

void DiaryFeature::apply_scroll_impulse(float px_per_sec) {
    _scroll_velocity_px_s += px_per_sec;
    if (_scroll_velocity_px_s > kScrollMaxPxS) {
        _scroll_velocity_px_s = kScrollMaxPxS;
    } else if (_scroll_velocity_px_s < -kScrollMaxPxS) {
        _scroll_velocity_px_s = -kScrollMaxPxS;
    }
    if (_scroll_physics_last_ms == 0) {
        _scroll_physics_last_ms = millis();
    }
}

bool DiaryFeature::tick_scroll_physics(unsigned long now_ms) {
    if (_scroll_velocity_px_s == 0.f) {
        _scroll_physics_last_ms = now_ms;
        return false;
    }
    if (_scroll_physics_last_ms == 0) {
        _scroll_physics_last_ms = now_ms;
        return false;
    }

    float dt = (now_ms - _scroll_physics_last_ms) / 1000.f;
    _scroll_physics_last_ms = now_ms;
    if (dt <= 0.f) {
        return false;
    }
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    const float before = _scroll_offset_y;
    _scroll_offset_y += _scroll_velocity_px_s * dt;
    clamp_scroll_offset();

    _scroll_velocity_px_s *= expf(-kScrollFrictionPerSec * dt);
    if (fabsf(_scroll_velocity_px_s) < kScrollStopPxS) {
        _scroll_velocity_px_s = 0.f;
    }

    return _scroll_velocity_px_s != 0.f || _scroll_offset_y != before;
}

void DiaryFeature::maybe_request_sync(unsigned long now_ms) {
    (void)now_ms;
    if (!gcal_auth_is_linked() || !wall_clock_is_synced()) {
        return;
    }
    int y, m, d;
    wall_clock_now_ymd(y, m, d);
    const int today = ymd_key(y, m, d);
    const unsigned long stale = 15UL * 60UL * 1000UL;
    const bool day_changed = _last_sync_ymd != 0 && _last_sync_ymd != today;
    const bool stale_time = diary_sync_last_ok_ms() == 0 ||
                            (millis() - diary_sync_last_ok_ms() > stale);
    if (diary_sync_busy()) {
        return;
    }
    if (day_changed || stale_time) {
        diary_sync_request();
    }
    _last_sync_ymd = today;
}

void DiaryFeature::onEnter() {
    _scroll_offset_y = 0.f;
    _scroll_velocity_px_s = 0.f;
    _scroll_physics_last_ms = 0;
    _scroll_gesture_cooldown_until_ms = millis() + GESTURE_ECHO_COOLDOWN_MS;
    _last_link_state = GcalLinkState::NotConfigured;
    _last_auth_status[0] = '\0';
    _qr_link_drawn = false;
    _sync_busy_last = false;
    _last_drawn_sync_ok_ms = 0;
    _buffer = diary_sync_day();
    if (_buffer.count == 0) {
        _buffer.clear();
    }
    refresh_content_height();
    maybe_request_sync(millis());
    setDirty();
}

void DiaryFeature::onTick(unsigned long now_ms) {
    const GcalLinkState st = gcal_auth_link_state();
    if (st == GcalLinkState::Linked) {
        _buffer = diary_sync_day();
        refresh_content_height();
        maybe_request_sync(now_ms);
    }
    const bool sync_busy = diary_sync_busy();
    if (sync_busy) {
        setDirty();
    } else if (_sync_busy_last) {
        _buffer = diary_sync_day();
        refresh_content_height();
        setDirty();
    }
    _sync_busy_last = sync_busy;

    if (st == GcalLinkState::Linked) {
        const unsigned long ok_ms = diary_sync_last_ok_ms();
        if (ok_ms != 0 && ok_ms != _last_drawn_sync_ok_ms) {
            _last_drawn_sync_ok_ms = ok_ms;
            _buffer = diary_sync_day();
            refresh_content_height();
            setDirty();
        }
        if (tick_scroll_physics(now_ms)) {
            setDirty();
        }
    }

    if (sync_busy) {
        return;
    }

    const bool linking = st == GcalLinkState::DeviceCodePending || st == GcalLinkState::ShowQr ||
                         st == GcalLinkState::PollingToken;
    if (linking) {
        const char* msg = gcal_auth_status_message();
        const char* url = gcal_auth_qr_url();
        const bool qr_ready = url && url[0] != '\0';
        if (st != _last_link_state) {
            _last_link_state = st;
            setDirty();
        }
        if (msg && strncmp(msg, _last_auth_status, sizeof(_last_auth_status)) != 0) {
            strncpy(_last_auth_status, msg, sizeof(_last_auth_status) - 1);
            _last_auth_status[sizeof(_last_auth_status) - 1] = '\0';
            setDirty();
        }
        if (qr_ready && !_qr_link_drawn) {
            _qr_link_drawn = true;
            setDirty();
        }
    } else {
        _last_link_state = st;
        _last_auth_status[0] = '\0';
        _qr_link_drawn = false;
    }
}

bool DiaryFeature::onInput(InputEvent event) {
    const GcalLinkState link = gcal_auth_link_state();

    if (event == InputEvent::Tap || event == InputEvent::DoubleTap) {
        if (link == GcalLinkState::PromptConnect || link == GcalLinkState::NeedsReauth ||
            link == GcalLinkState::LinkFailed) {
#if DEBUG_TOUCH
            Serial.println("[DIARY] tap connect");
#endif
            _qr_link_drawn = false;
            _last_auth_status[0] = '\0';
            gcal_auth_request_connect();
            setDirty();
            return true;
        }
        if (link == GcalLinkState::PollingToken || link == GcalLinkState::ShowQr) {
            gcal_auth_cancel_connect();
            setDirty();
            return true;
        }
        return false;
    }

    if (link != GcalLinkState::Linked) {
        return false;
    }

    if (event != InputEvent::SwipeUp && event != InputEvent::SwipeDown) {
        return false;
    }

    if (_content_height_px > kVisibleListHeight) {
        // Finger up → content moves up → reveal later events (increase scroll offset).
        const float impulse = event == InputEvent::SwipeUp ? -kScrollImpulsePxS : kScrollImpulsePxS;
        apply_scroll_impulse(impulse);
        setDirty();
    }
    return true;
}

void DiaryFeature::draw_connect_prompt(lgfx::LGFX_Device& gfx, int cx) {
    gfx.setTextDatum(textdatum_t::middle_center);
    gfx.setTextSize(kEventTextScale);
    if (!wifi_station_is_connected()) {
        gfx.setTextColor(TFT_ORANGE, TFT_BLACK);
        gfx.drawString("WiFi required", cx, 64);
        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.setTextSize(kMetaTextScale);
        gfx.drawString("serial: wifi add", cx, 88);
    } else {
        gfx.setTextColor(TFT_WHITE, TFT_BLACK);
        gfx.drawString("Connect Google", cx, 64);
        gfx.drawString("Calendar?", cx, 84);
        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.setTextSize(kMetaTextScale);
        gfx.drawString("tap to link", cx, 108);
    }
    const char* msg = gcal_auth_status_message();
    if (msg && msg[0]) {
        gfx.drawString(msg, cx, 132);
    }
}

void DiaryFeature::draw_qr_link(lgfx::LGFX_Device& gfx, int cx) {
    gfx.setTextDatum(textdatum_t::middle_center);
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.setTextSize(1);
    gfx.drawString("Scan with phone", cx, 24);
    const char* url = gcal_auth_qr_url();
    if (url && url[0]) {
        diary_qr_draw(gfx, url, cx, 108, 2);
    }
    gfx.drawString(gcal_auth_user_code(), cx, 188);
    const char* msg = gcal_auth_status_message();
    if (msg && msg[0]) {
        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.drawString(msg, cx, 208);
    }
    gfx.drawString("tap cancel", cx, 224);
}

void DiaryFeature::draw_agenda(lgfx::LGFX_Device& gfx, int cx) {
    const int cy = kDialCy;
    char header[40];
    if (_buffer.year != 0) {
        format_agenda_date_header(_buffer.year, _buffer.month, _buffer.day, header, sizeof(header));
    } else {
        int y, m, d;
        wall_clock_now_ymd(y, m, d);
        format_agenda_date_header(y, m, d, header, sizeof(header));
    }

    gfx.drawCircle(cx, cy, kSafeRadius, 0x3186);

    gfx.setTextDatum(textdatum_t::middle_center);
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.setTextSize(kEventTextScale);
    gfx.drawString(header, cx, kDateHeaderY);

    const int sep_y = kListTop - 5;
    const int sep_hw = chord_half_width_at_y(sep_y, cx, cy, kSafeRadius - 2);
    if (sep_hw > 0) {
        gfx.drawFastHLine(cx - sep_hw, sep_y, sep_hw * 2, 0x4208);
    }

    if (diary_sync_busy()) {
        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.drawString("syncing...", cx, kListTop + 28);
    } else if (diary_sync_last_error()[0]) {
        gfx.setTextColor(TFT_ORANGE, TFT_BLACK);
        gfx.setTextSize(kMetaTextScale);
        gfx.drawString(diary_sync_last_error(), cx, kListTop + 28);
    }

    if (_buffer.count == 0 && !diary_sync_busy()) {
        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.setTextSize(kEventTextScale);
        gfx.drawString("Nothing scheduled", cx, kListTop + 40);
    }

    const int list_mid_y = kListTop + kVisibleListHeight / 2;
    const int list_hw = chord_half_width_at_y(list_mid_y, cx, cy, kSafeRadius - 6);
    const int clip_x = cx - list_hw;
    const int clip_w = list_hw * 2;
    gfx.setClipRect(clip_x, kListTop, clip_w, kVisibleListHeight);

    const int scroll_y = (int)(_scroll_offset_y + 0.5f);
    const int time_x = clip_x + 8;
    const int title_x = clip_x + kTimeColWidth + 10;

    for (size_t i = 0; i < _buffer.count; ++i) {
        const DiaryEventBuf& ev = _buffer.events[i];
        const int y = kListTop + (int)i * kRowHeight - scroll_y;
        if (y + kRowHeight < kListTop || y > kListBottom) {
            continue;
        }

        const int row_mid = y + kRowHeight / 2;
        const int row_hw = chord_half_width_at_y(row_mid, cx, cy, kSafeRadius - 8);
        if (row_hw > 8) {
            gfx.drawFastHLine(cx - row_hw + 6, y + kRowHeight - 1, row_hw * 2 - 12, 0x2104);
        }

        char time_buf[16];
        if (ev.all_day) {
            snprintf(time_buf, sizeof(time_buf), "All day");
        } else {
            format_time(ev.start_min, time_buf, sizeof(time_buf));
        }

        gfx.setTextDatum(textdatum_t::middle_left);
        gfx.setTextColor(TFT_CYAN, TFT_BLACK);
        gfx.setTextSize(kEventTextScale);
        gfx.drawString(time_buf, time_x, row_mid);

        gfx.setTextColor(TFT_WHITE, TFT_BLACK);
        gfx.setTextSize(kEventTextScale);
        gfx.drawString(ev.title, title_x, row_mid);
    }

    gfx.clearClipRect();
}

void DiaryFeature::onDraw(lgfx::LGFX_Device& gfx) {
    const int cx = DISPLAY_WIDTH / 2;
    gfx.fillScreen(TFT_BLACK);

    const GcalLinkState link = gcal_auth_link_state();
    switch (link) {
        case GcalLinkState::NotConfigured:
            gfx.setTextDatum(textdatum_t::middle_center);
            gfx.setTextColor(TFT_ORANGE, TFT_BLACK);
            gfx.drawString("Diary", cx, 60);
            gfx.drawString("OAuth not configured", cx, 90);
            gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
            gfx.drawString("gcal_config_private.h", cx, 110);
            break;
        case GcalLinkState::PromptConnect:
        case GcalLinkState::NeedsReauth:
        case GcalLinkState::LinkFailed:
            draw_connect_prompt(gfx, cx);
            break;
        case GcalLinkState::DeviceCodePending:
        case GcalLinkState::ShowQr:
        case GcalLinkState::PollingToken:
            draw_qr_link(gfx, cx);
            break;
        case GcalLinkState::Linked:
            draw_agenda(gfx, cx);
            break;
    }
}

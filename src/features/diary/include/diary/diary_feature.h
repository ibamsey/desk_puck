#ifndef DIARY_DIARY_FEATURE_H
#define DIARY_DIARY_FEATURE_H

#include "diary/diary_types.h"
#include "diary/gcal_auth.h"
#include "feature.h"

class DiaryFeature : public Feature {
public:
    const char* name() const override { return "Diary"; }

    void onEnter() override;
    void onTick(unsigned long now_ms) override;
    bool onInput(InputEvent event) override;
    void onDraw(lgfx::LGFX_Device& gfx) override;

private:
    int max_scroll_px() const;
    void clamp_scroll_offset();
    void apply_scroll_impulse(float px_per_sec);
    bool tick_scroll_physics(unsigned long now_ms);
    void refresh_content_height();
    void maybe_request_sync(unsigned long now_ms);
    void draw_connect_prompt(lgfx::LGFX_Device& gfx, int cx);
    void draw_qr_link(lgfx::LGFX_Device& gfx, int cx);
    void draw_agenda(lgfx::LGFX_Device& gfx, int cx);

    DiaryDayBuffer _buffer{};
    float _scroll_offset_y = 0.f;
    float _scroll_velocity_px_s = 0.f;
    unsigned long _scroll_physics_last_ms = 0;
    int _content_height_px = 0;
    unsigned long _scroll_gesture_cooldown_until_ms = 0;
    int _last_sync_ymd = 0;

    GcalLinkState _last_link_state = GcalLinkState::NotConfigured;
    char _last_auth_status[64] = {};
    bool _qr_link_drawn = false;
    bool _sync_busy_last = false;
    unsigned long _last_drawn_sync_ok_ms = 0;
};

#endif // DIARY_DIARY_FEATURE_H

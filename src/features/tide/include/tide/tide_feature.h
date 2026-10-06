#ifndef TIDE_TIDE_FEATURE_H
#define TIDE_TIDE_FEATURE_H

#include "feature.h"
#include "tide/tide_feed.h"

class TideFeature : public Feature {
public:
    const char* name() const override { return "Tide"; }

    void onEnter() override;
    void onExit() override;
    void onTick(unsigned long now_ms) override;
    void onDraw(lgfx::LGFX_Device& gfx) override;
    bool onInput(InputEvent event) override;

private:
    void request_full();
    float second_fraction(unsigned long now_ms, int second) const;

    TideFeedSnapshot _snap{};
    bool _had_valid = false;

    int _last_hour = -1;
    int _last_minute = -1;
    int _last_second = -1;
    int _frac_anchor_second = -1;
    unsigned long _frac_anchor_ms = 0;
    unsigned long _smooth_last_draw_ms = 0;

    int _face_yday = -1;
    int _face_year = -1;
};

#endif // TIDE_TIDE_FEATURE_H

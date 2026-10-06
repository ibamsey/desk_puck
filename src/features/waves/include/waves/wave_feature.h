#ifndef WAVES_WAVE_FEATURE_H
#define WAVES_WAVE_FEATURE_H

#include "feature.h"
#include "waves/wave_compositor.h"
#include "waves/wave_feed.h"

class WaveFeature : public Feature {
public:
    const char* name() const override { return "Waves"; }

    void onEnter() override;
    void onExit() override;
    void onTick(unsigned long now_ms) override;
    void onDraw(lgfx::LGFX_Device& gfx) override;
    bool onInput(InputEvent event) override;

private:
    WaveFeedSnapshot _snap{};
    WaveCompositor _compositor;
    bool _had_valid = false;
    unsigned long _anim_origin_ms = 0;
};

#endif // WAVES_WAVE_FEATURE_H

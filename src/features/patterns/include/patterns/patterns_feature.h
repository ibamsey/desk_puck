#ifndef PATTERNS_PATTERNS_FEATURE_H
#define PATTERNS_PATTERNS_FEATURE_H

#include "feature.h"

class PatternsFeature : public Feature {
public:
    const char* name() const override { return "Patterns"; }

    void onEnter() override;
    bool onInput(InputEvent event) override;
    void onDraw(lgfx::LGFX_Device& gfx) override;

private:
    void regenerate();
    uint32_t _seed = 0;
};

#endif // PATTERNS_PATTERNS_FEATURE_H

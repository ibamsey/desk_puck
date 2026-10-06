#ifndef TIDE_TIDE_GAUGE_H
#define TIDE_TIDE_GAUGE_H

#include "tide/tide_feed.h"
#include "LGFX_config.h"

struct TidePolarContext {
    time_t now_epoch = 0;
    /** Sub-second fraction for smooth hand rotation (0..1). */
    float second_frac = 0.0f;
};

void tide_polar_draw(lgfx::LGFX_Device& gfx, const TideFeedSnapshot& snap, const TidePolarContext& ctx);

#endif // TIDE_TIDE_GAUGE_H

#ifndef CUBE_CUBE_FEATURE_H
#define CUBE_CUBE_FEATURE_H

#include "feature.h"

class CubeFeature : public Feature {
public:
    const char* name() const override { return "Cube"; }

    void onEnter() override;
    void onTick(unsigned long now_ms) override;
    void onDraw(lgfx::LGFX_Device& gfx) override;
    bool onInput(InputEvent event) override;

private:
    void kick();
    void schedule_next_random_kick(unsigned long now_ms);

    float _angle_x = 0.0f;
    float _angle_y = 0.0f;
    float _angle_z = 0.0f;
    float _omega_x = 0.0f;
    float _omega_y = 0.0f;
    float _omega_z = 0.0f;

    unsigned long _last_physics_ms = 0;
    unsigned long _next_random_kick_ms = 0;
};

#endif // CUBE_CUBE_FEATURE_H

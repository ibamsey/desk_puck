#include "cube/cube_feature.h"

#include "config.h"

#include <esp_random.h>
#include <math.h>

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kKickMinRadS = 2.8f;
constexpr float kKickMaxRadS = 5.2f;
constexpr float kSpinStopRadS = 0.04f;
constexpr float kAngularDrag = 0.92f; // per second (exponential)
constexpr unsigned long kPhysicsIntervalMs = 33;
constexpr unsigned long kRandomKickMinMs = 120000;
constexpr unsigned long kRandomKickSpanMs = 180000;

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Vec2 {
    int x;
    int y;
};

struct Face {
    uint8_t v[4];
};

static Vec3 rotate_euler(Vec3 p, float ax, float ay, float az) {
    const float cx = cosf(ax);
    const float sx = sinf(ax);
    const float cy = cosf(ay);
    const float sy = sinf(ay);
    const float cz = cosf(az);
    const float sz = sinf(az);

    float x = p.x;
    float y = p.y;
    float z = p.z;

    float y1 = y * cx - z * sx;
    float z1 = y * sx + z * cx;
    y = y1;
    z = z1;

    float x1 = x * cy + z * sy;
    z1 = -x * sy + z * cy;
    x = x1;
    z = z1;

    x1 = x * cz - y * sz;
    y1 = x * sz + y * cz;
    x = x1;
    y = y1;

    return {x, y, z};
}

static Vec3 face_normal(const Vec3* w, const Face& f) {
    const Vec3& a = w[f.v[0]];
    const Vec3& b = w[f.v[1]];
    const Vec3& c = w[f.v[2]];
    const float ux = b.x - a.x;
    const float uy = b.y - a.y;
    const float uz = b.z - a.z;
    const float vx = c.x - a.x;
    const float vy = c.y - a.y;
    const float vz = c.z - a.z;
    Vec3 n = {
        uy * vz - uz * vy,
        uz * vx - ux * vz,
        ux * vy - uy * vx,
    };
    const float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len > 1e-6f) {
        n.x /= len;
        n.y /= len;
        n.z /= len;
    }
    return n;
}

static uint16_t rgb565_grey(uint8_t g) {
    const uint8_t r5 = g >> 3;
    const uint8_t g6 = g >> 2;
    const uint8_t b5 = g >> 3;
    return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

static uint16_t wire_color_for_depth(float avg_z, float half_size) {
    const float t = (avg_z / half_size + 1.0f) * 0.5f;
    const float clamped = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    const uint8_t g = (uint8_t)(80.0f + clamped * 175.0f);
    return rgb565_grey(g);
}

static float rand_unit(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return (float)(state & 0xffff) / 32767.5f - 1.0f;
}

static float spin_speed(float ox, float oy, float oz) {
    return sqrtf(ox * ox + oy * oy + oz * oz);
}

} // namespace

void CubeFeature::kick() {
    uint32_t state = esp_random();
    if (state == 0) {
        state = 1;
    }

    const float t = (float)(esp_random() % 1000) / 1000.0f;
    const float mag = kKickMinRadS + t * (kKickMaxRadS - kKickMinRadS);

    float dx = rand_unit(state);
    float dy = rand_unit(state);
    float dz = rand_unit(state);
    const float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len > 1e-4f) {
        dx /= len;
        dy /= len;
        dz /= len;
    }

    _omega_x = dx * mag;
    _omega_y = dy * mag;
    _omega_z = dz * mag;
    setDirty();
}

void CubeFeature::schedule_next_random_kick(unsigned long now_ms) {
    const unsigned long delay = kRandomKickMinMs + (esp_random() % kRandomKickSpanMs);
    _next_random_kick_ms = now_ms + delay;
}

void CubeFeature::onEnter() {
    _angle_x = _angle_y = _angle_z = 0.0f;
    _omega_x = _omega_y = _omega_z = 0.0f;
    _last_physics_ms = millis();
    kick();
    schedule_next_random_kick(_last_physics_ms);
    setDirty();
}

bool CubeFeature::onInput(InputEvent event) {
    if (event == InputEvent::Tap || event == InputEvent::DoubleTap) {
        kick();
        return true;
    }
    return false;
}

void CubeFeature::onTick(unsigned long now_ms) {
    if (now_ms >= _next_random_kick_ms) {
        kick();
        schedule_next_random_kick(now_ms);
    }

    if (_last_physics_ms == 0) {
        _last_physics_ms = now_ms;
    }

    const unsigned long elapsed = now_ms - _last_physics_ms;
    if (elapsed < kPhysicsIntervalMs) {
        return;
    }

    const float dt = (float)elapsed * 0.001f;
    _last_physics_ms = now_ms;

    const float speed_before = spin_speed(_omega_x, _omega_y, _omega_z);
    if (speed_before < kSpinStopRadS) {
        _omega_x = _omega_y = _omega_z = 0.0f;
        return;
    }

    _angle_x += _omega_x * dt;
    _angle_y += _omega_y * dt;
    _angle_z += _omega_z * dt;

    if (_angle_x > kPi) {
        _angle_x -= 2.0f * kPi;
    } else if (_angle_x < -kPi) {
        _angle_x += 2.0f * kPi;
    }
    if (_angle_y > kPi) {
        _angle_y -= 2.0f * kPi;
    } else if (_angle_y < -kPi) {
        _angle_y += 2.0f * kPi;
    }
    if (_angle_z > kPi) {
        _angle_z -= 2.0f * kPi;
    } else if (_angle_z < -kPi) {
        _angle_z += 2.0f * kPi;
    }

    const float decay = powf(kAngularDrag, dt);
    _omega_x *= decay;
    _omega_y *= decay;
    _omega_z *= decay;

    setDirty();
}

void CubeFeature::onDraw(lgfx::LGFX_Device& gfx) {
    static const Vec3 kUnit[] = {
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
    };

    static const Face kFaces[] = {
        {0, 1, 2, 3},
        {4, 5, 6, 7},
        {1, 5, 6, 2},
        {4, 0, 3, 7},
        {3, 2, 6, 7},
        {0, 1, 5, 4},
    };

    static const uint8_t kEdges[][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };

    const int cx = DISPLAY_WIDTH / 2;
    const int cy = DISPLAY_HEIGHT / 2;
    const float half = 72.0f;
    const float perspective = 3.2f;

    Vec3 world[8];
    Vec2 screen[8];
    float depth[8];

    for (int i = 0; i < 8; i++) {
        world[i] = rotate_euler(kUnit[i], _angle_x, _angle_y, _angle_z);
        const float inv_z = 1.0f / (perspective - world[i].z);
        screen[i].x = cx + (int)(world[i].x * half * inv_z);
        screen[i].y = cy + (int)(world[i].y * half * inv_z);
        depth[i] = world[i].z;
    }

    struct FaceDraw {
        int index;
        float avg_z;
    };
    FaceDraw order[6];
    for (int i = 0; i < 6; i++) {
        const Face& f = kFaces[i];
        order[i].index = i;
        order[i].avg_z = (depth[f.v[0]] + depth[f.v[1]] + depth[f.v[2]] + depth[f.v[3]]) * 0.25f;
    }

    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 6; j++) {
            if (order[i].avg_z > order[j].avg_z) {
                FaceDraw tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    gfx.fillScreen(TFT_BLACK);

    const Vec3 light = {0.35f, 0.55f, 0.76f};
    const float light_len = sqrtf(light.x * light.x + light.y * light.y + light.z * light.z);

    for (int o = 0; o < 6; o++) {
        const Face& f = kFaces[order[o].index];
        const Vec3 n = face_normal(world, f);
        if (n.z <= 0.02f) {
            continue;
        }

        const float diffuse =
            (n.x * light.x + n.y * light.y + n.z * light.z) / light_len;
        const float lit = diffuse < 0.0f ? 0.0f : diffuse;
        const uint8_t grey = (uint8_t)(40.0f + lit * 140.0f);
        const uint16_t fill = rgb565_grey(grey);

        const Vec2& p0 = screen[f.v[0]];
        const Vec2& p1 = screen[f.v[1]];
        const Vec2& p2 = screen[f.v[2]];
        const Vec2& p3 = screen[f.v[3]];

        gfx.fillTriangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, fill);
        gfx.fillTriangle(p0.x, p0.y, p2.x, p2.y, p3.x, p3.y, fill);
    }

    for (const auto& e : kEdges) {
        const float avg_z = (depth[e[0]] + depth[e[1]]) * 0.5f;
        const uint16_t edge = wire_color_for_depth(avg_z, 1.0f);
        gfx.drawLine(screen[e[0]].x, screen[e[0]].y, screen[e[1]].x, screen[e[1]].y, edge);
    }
}

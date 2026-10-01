#ifndef CLOCK_ASSET_FACE_H
#define CLOCK_ASSET_FACE_H

#include "clock/clock_faces.h"
#include "clock/generated/watch_face_manifest.h"
#include "LGFX_config.h"

class AssetFaceRuntime {
public:
    bool load(int asset_index, const AssetFaceMeta* meta);
    void unload();
    bool is_loaded() const { return _dial != nullptr; }
    int loaded_asset_index() const { return _asset_index; }

    void draw(lgfx::LGFX_Device& gfx, const AnalogClockState& state);

private:
    struct HandBuffer {
        lgfx::argb8888_t* pixels = nullptr;
        uint16_t width = 0;
        uint16_t height = 0;
        int16_t pivot_x = 0;
        int16_t pivot_y = 0;
        float offset_deg = 0;
        bool active = false;
    };

    struct LoadedSubdial {
        HandBuffer hand;
        int16_t x = 0;
        int16_t y = 0;
        SubdialRole role = SubdialRole::ChronoSecond;
    };

    bool load_hand_chunk(const uint8_t* blob, size_t& offset, const WatchFaceHandMeta& meta, HandBuffer& out);
    bool load_hub_chunk(const uint8_t* blob, size_t& offset, const WatchFaceHubMeta& meta, HandBuffer& out);
    void draw_hand(lgfx::LGFX_Device& gfx, const HandBuffer& hand, float angle_deg) const;
    void draw_hand_at(lgfx::LGFX_Device& gfx, const HandBuffer& hand, int pivot_x, int pivot_y,
                      float angle_deg) const;
    void draw_hub(lgfx::LGFX_Device& gfx) const;
    float subdial_angle(SubdialRole role, const AnalogClockState& state) const;

    uint16_t* _dial = nullptr;
    HandBuffer _hour;
    HandBuffer _minute;
    HandBuffer _second;
    HandBuffer _hub;
    LoadedSubdial _subdials[kMaxSubdials];
    uint8_t _subdial_count = 0;
    int16_t _pivot_x = 120;
    int16_t _pivot_y = 120;
    int _asset_index = -1;
    bool _has_second = false;
    bool _has_hub = false;
};

#endif // CLOCK_ASSET_FACE_H

#ifndef CLOCK_ASSET_FACE_H
#define CLOCK_ASSET_FACE_H

#include "clock/clock_faces.h"
#include "clock/generated/watch_face_manifest.h"
#include "LGFX_config.h"

class AssetFaceRuntime {
public:
    bool load(int asset_index, const AssetFaceMeta* meta);
    void unload();
    /** Frees the optional smooth-tick underlay (e.g. when leaving the clock feature). */
    void release_underlay();
    bool ensure_underlay();
    bool is_loaded() const { return _dial != nullptr; }
    int loaded_asset_index() const { return _asset_index; }

    void draw(lgfx::LGFX_Device& gfx, const AnalogClockState& state, ClockHandMask mask,
              const AnalogClockState* prev_state);

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
    void draw_hand(lgfx::LovyanGFX& gfx, const HandBuffer& hand, float angle_deg) const;
    void draw_hand_at(lgfx::LovyanGFX& gfx, const HandBuffer& hand, int pivot_x, int pivot_y,
                      float angle_deg) const;
    void draw_hub(lgfx::LovyanGFX& gfx) const;
    float subdial_angle(SubdialRole role, const AnalogClockState& state) const;
    int hand_cover_radius(const HandBuffer& hand) const;
    void blit_buffer_rect(lgfx::LGFX_Device& gfx, const uint16_t* src, int x, int y, int w, int h) const;
    void restore_subdial_patch(lgfx::LGFX_Device& gfx, const LoadedSubdial& sd) const;
    void rebuild_underlay(const AnalogClockState& state);
    void second_sweep_union_rect(const AnalogClockState& state, const AnalogClockState& prev, int& ux,
                                 int& uy, int& uw, int& uh) const;
    void rect_union(int& ux, int& uy, int& uw, int& uh, int x, int y, int w, int h) const;
    void unpaint_hand_from_background(lgfx::LGFX_Device& gfx, const HandBuffer& hand, int pivot_x, int pivot_y,
                                      float angle_deg, const uint16_t* bg_rgb565) const;
    void patch_underlay_hands(lgfx::LGFX_Device& gfx, const AnalogClockState& state, const AnalogClockState& prev,
                              ClockHandMask mask) const;
    void draw_second_sweep(lgfx::LGFX_Device& gfx, const AnalogClockState& state, const AnalogClockState& prev,
                           bool draw_hub_cap) const;
    void draw_center_wall_hands(lgfx::LovyanGFX& gfx, const AnalogClockState& state, bool hour, bool minute,
                                bool second) const;
    void hand_dirty_rect(int pivot_x, int pivot_y, const HandBuffer& hand, float angle_deg, int& out_x,
                         int& out_y, int& out_w, int& out_h) const;

    uint16_t* _dial = nullptr;
    uint16_t* _underlay = nullptr;
    bool _underlay_valid = false;
    float _underlay_hour_angle = -999.0f;
    float _underlay_minute_angle = -999.0f;
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

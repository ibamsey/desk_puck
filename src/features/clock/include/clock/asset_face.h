#ifndef CLOCK_ASSET_FACE_H
#define CLOCK_ASSET_FACE_H

#include "clock/clock_digital.h"
#include "clock/clock_faces.h"
#include "clock/generated/watch_face_manifest.h"
#include "LGFX_config.h"

class AssetFaceRuntime {
public:
    bool load(int asset_index, const AssetFaceMeta* meta);
    void unload();
    bool is_loaded() const { return _blob != nullptr; }
    int loaded_asset_index() const { return _asset_index; }

    /**
     * One compositor path for every frame.
     * _static is written only by rebuild_static(). Fast hands composite from
     * _static into a RAM patch (rotate+AA) then one pushImage to the panel.
     * A missing static layer never skips the second hand.
     */
    void present(lgfx::LGFX_Device& gfx, const AnalogClockState& state, bool force_full);

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
    void composite_hand_on_panel(lgfx::LGFX_Device& gfx, const HandBuffer& hand, int pivot_x, int pivot_y,
                                 float angle_deg, bool has_prev, float prev_angle_deg) const;
    bool ensure_hand_patch(size_t pixel_count) const;
    void draw_hub(lgfx::LovyanGFX& gfx) const;
    void draw_fast_hands(lgfx::LGFX_Device& gfx, const AnalogClockState& state, bool partial_update) const;
    float subdial_angle(SubdialRole role, const AnalogClockState& state) const;
    bool ensure_static();
    bool rebuild_static(const AnalogClockState& state);
    void blit_dial_from_flash(lgfx::LGFX_Device& gfx) const;
    void hand_dirty_rect(int pivot_x, int pivot_y, const HandBuffer& hand, float angle_deg, int& out_x,
                         int& out_y, int& out_w, int& out_h) const;
    void clip_rect(int& x, int& y, int& w, int& h) const;
    void rect_union(int& ux, int& uy, int& uw, int& uh, int x, int y, int w, int h) const;
    void remember_shown(const AnalogClockState& state);
    void present_digital_compositor(lgfx::LGFX_Device& gfx, const AnalogClockState& state, bool force_full);
    clock_digital::DigitalReadoutStyle digital_readout_style() const;
    void draw_digital_on_layer(lgfx::LovyanGFX& layer, const ClockWallTime& wall) const;
    void second_hand_dirty_union(const AnalogClockState& state, bool merge_prev, int& ux, int& uy, int& uw,
                                 int& uh) const;
    void second_rotation_pivot(int& x, int& y) const;
    bool push_band_patch(lgfx::LGFX_Device& gfx, int x, int y, int w, int h) const;
    void push_readout_bands_excluding(lgfx::LGFX_Device& gfx, int ex, int ey, int ew, int eh) const;
    void draw_weather_icon(lgfx::LovyanGFX& gfx) const;

    const uint8_t* _blob = nullptr;
    uint16_t* _static = nullptr;
    bool _static_valid = false;
    int _static_hour = -1;
    int _static_minute = -1;
    HandBuffer _hour;
    HandBuffer _minute;
    HandBuffer _second;
    HandBuffer _hub;
    HandBuffer _weather;
    LoadedSubdial _subdials[kMaxSubdials];
    uint8_t _subdial_count = 0;
    int16_t _pivot_x = 120;
    int16_t _pivot_y = 120;
    int16_t _second_dial_pivot_x = kDialPivotUseFace;
    int16_t _second_dial_pivot_y = kDialPivotUseFace;
    int _asset_index = -1;
    bool _has_second = false;
    bool _has_hub = false;
    bool _has_digital = false;
    bool _has_weather = false;
    AssetDigitalReadoutMeta _digital_meta{};
    AssetWeatherIconMeta _weather_meta{};
    bool _has_shown_second = false;
    float _shown_second_angle = 0.0f;
    bool _has_shown_chrono = false;
    unsigned long _shown_chrono_ms = 0;
    mutable uint16_t* _hand_patch = nullptr;
    mutable size_t _hand_patch_pixels = 0;
};

#endif // CLOCK_ASSET_FACE_H

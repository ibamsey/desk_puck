#ifndef WATCH_FACE_MANIFEST_H
#define WATCH_FACE_MANIFEST_H

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

enum class WatchFaceHandKind : uint8_t { Hour, Minute, Second, Chrono };

enum class SubdialRole : uint8_t { ChronoSecond = 0, ChronoMinute = 1, WallSecond = 2 };

static constexpr int16_t kDialPivotUseFace = -32768;

struct WatchFaceHandMeta {
    uint16_t width;
    uint16_t height;
    int16_t pivot_x;
    int16_t pivot_y;
    float offset_deg;
    WatchFaceHandKind kind;
    /** Screen pivot for rotation; kDialPivotUseFace = use face pivot from AssetFaceMeta. */
    int16_t dial_pivot_x;
    int16_t dial_pivot_y;
};

struct WatchFaceHubMeta {
    uint16_t width;
    uint16_t height;
    int16_t pivot_x;
    int16_t pivot_y;
};

struct SubdialMeta {
    WatchFaceHandMeta hand;
    int16_t x;
    int16_t y;
    SubdialRole role;
};

static constexpr uint8_t kMaxSubdials = 3;

struct AssetDigitalReadoutMeta {
    int16_t time_x;
    int16_t time_y;
    int16_t date_x;
    int16_t date_y;
    uint16_t time_color;
    uint16_t date_color;
    uint16_t bg_color;
    uint8_t time_text_size;
    uint8_t date_text_size;
    uint8_t flags;
};

struct AssetWeatherIconMeta {
    int16_t x;
    int16_t y;
    uint8_t grid_cols;
    uint8_t grid_rows;
    uint16_t sheet_w;
    uint16_t sheet_h;
};

struct AssetFaceMeta {
    const char* id;
    const char* name;
    const uint8_t* blob;
    size_t blob_size;
    int16_t pivot_x;
    int16_t pivot_y;
    uint16_t flags;
    WatchFaceHandMeta hour;
    WatchFaceHandMeta minute;
    WatchFaceHandMeta second;
    uint8_t subdial_count;
    SubdialMeta subdials[kMaxSubdials];
    WatchFaceHubMeta hub;
    AssetDigitalReadoutMeta digital;
    AssetWeatherIconMeta weather;
    bool has_second() const { return (flags & 0x0001) != 0; }
    bool has_hub() const { return (flags & 0x0002) != 0; }
    bool has_subdials() const { return (flags & 0x0004) != 0; }
    bool is_chronograph() const { return (flags & 0x0008) != 0; }
    bool has_digital_readout() const { return (flags & 0x0010) != 0; }
    bool has_weather_icon() const { return (flags & 0x0020) != 0; }
};

namespace watch_face_assets {
extern const uint8_t kBlob_3rd_test_run[];
extern const size_t kBlobSize_3rd_test_run;
extern const uint8_t kBlob_bell_ross[];
extern const size_t kBlobSize_bell_ross;
extern const uint8_t kBlob_classic_chrono[];
extern const size_t kBlobSize_classic_chrono;
extern const uint8_t kBlob_dg_chronometer_bursted_steel_and_gold[];
extern const size_t kBlobSize_dg_chronometer_bursted_steel_and_gold;
extern const uint8_t kBlob_koboldpt[];
extern const size_t kBlobSize_koboldpt;
extern const uint8_t kBlob_raggazo_r1_grey_blue[];
extern const size_t kBlobSize_raggazo_r1_grey_blue;
extern const uint8_t kBlob_steampunk[];
extern const size_t kBlobSize_steampunk;
extern const uint8_t kBlob_wayfinder[];
extern const size_t kBlobSize_wayfinder;
}

inline size_t asset_face_count() { return 8; }

inline const AssetFaceMeta* asset_face_meta(size_t index) {
    static const AssetFaceMeta kFaces[] = {
  { "3rd-test-run", "3rd test run.", watch_face_assets::kBlob_3rd_test_run, watch_face_assets::kBlobSize_3rd_test_run, 120, 120, (uint16_t)49, { 26, 85, 13, 72, 0.00f, WatchFaceHandKind::Hour, -32768, -32768 }, { 24, 98, 12, 87, 0.00f, WatchFaceHandKind::Minute, -32768, -32768 }, { 25, 110, 12, 80, 0.00f, WatchFaceHandKind::Second, -32768, -32768 }, 0, { { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond } }, { 0, 0, 0, 0 }, { 120, 168, 120, 151, 12159, 12159, 0, 3, 2, 51 }, { 120, 73, 3, 3, 54, 54 } },
  { "bell-ross", "Bell & Ross BR S", watch_face_assets::kBlob_bell_ross, watch_face_assets::kBlobSize_bell_ross, 120, 120, (uint16_t)17, { 17, 69, 8, 68, 0.00f, WatchFaceHandKind::Hour, -32768, -32768 }, { 15, 99, 7, 98, 0.00f, WatchFaceHandKind::Minute, -32768, -32768 }, { 9, 27, 4, 26, 0.00f, WatchFaceHandKind::Second, 120, 155 }, 0, { { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond } }, { 0, 0, 0, 0 }, { 120, 108, 167, 167, 65535, 19049, 0, 1, 2, 6 }, { 0, 0, 0, 0, 0, 0 } },
  { "classic-chrono", "Classic Chrono", watch_face_assets::kBlob_classic_chrono, watch_face_assets::kBlobSize_classic_chrono, 120, 120, (uint16_t)14, { 12, 56, 6, 55, 0.00f, WatchFaceHandKind::Hour, -32768, -32768 }, { 8, 84, 4, 83, 0.00f, WatchFaceHandKind::Minute, -32768, -32768 }, { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Second, -32768, -32768 }, 3, { { { 6, 40, 3, 39, 0.00f, WatchFaceHandKind::Chrono, -32768, -32768 }, 120, 168, SubdialRole::WallSecond }, { { 6, 44, 3, 43, 0.00f, WatchFaceHandKind::Chrono, -32768, -32768 }, 52, 120, SubdialRole::ChronoMinute }, { { 6, 48, 3, 47, 0.00f, WatchFaceHandKind::Chrono, -32768, -32768 }, 168, 120, SubdialRole::ChronoSecond } }, { 20, 20, 10, 10 }, { 0, 0, 120, 120, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } },
  { "dg-chronometer-bursted-steel-and-gold", "D&G Chronometer Bursted Steel and Gold", watch_face_assets::kBlob_dg_chronometer_bursted_steel_and_gold, watch_face_assets::kBlobSize_dg_chronometer_bursted_steel_and_gold, 120, 120, (uint16_t)29, { 18, 85, 9, 64, 0.00f, WatchFaceHandKind::Hour, -32768, -32768 }, { 22, 110, 11, 91, 0.00f, WatchFaceHandKind::Minute, -32768, -32768 }, { 10, 110, 5, 79, 0.00f, WatchFaceHandKind::Second, -32768, -32768 }, 2, { { { 16, 40, 8, 31, 0.00f, WatchFaceHandKind::Chrono, -32768, -32768 }, 66, 120, SubdialRole::WallSecond }, { { 16, 40, 8, 31, 0.00f, WatchFaceHandKind::Chrono, -32768, -32768 }, 120, 172, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond } }, { 0, 0, 0, 0 }, { 120, 108, 190, 120, 65535, 65535, 0, 1, 2, 6 }, { 0, 0, 0, 0, 0, 0 } },
  { "koboldpt", "Kobold Phantom Tactical", watch_face_assets::kBlob_koboldpt, watch_face_assets::kBlobSize_koboldpt, 120, 120, (uint16_t)13, { 21, 72, 10, 61, 0.00f, WatchFaceHandKind::Hour, -32768, -32768 }, { 18, 95, 9, 86, 0.00f, WatchFaceHandKind::Minute, -32768, -32768 }, { 13, 110, 6, 86, 0.00f, WatchFaceHandKind::Second, -32768, -32768 }, 3, { { { 13, 110, 6, 86, 0.00f, WatchFaceHandKind::Chrono, -32768, -32768 }, 124, 116, SubdialRole::WallSecond }, { { 12, 31, 6, 25, 0.00f, WatchFaceHandKind::Chrono, -32768, -32768 }, 120, 71, SubdialRole::ChronoMinute }, { { 12, 31, 6, 25, 0.00f, WatchFaceHandKind::Chrono, -32768, -32768 }, 120, 168, SubdialRole::ChronoSecond } }, { 0, 0, 0, 0 }, { 0, 0, 120, 120, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } },
  { "raggazo-r1-grey-blue", "Raggazo R1 Grey Blue", watch_face_assets::kBlob_raggazo_r1_grey_blue, watch_face_assets::kBlobSize_raggazo_r1_grey_blue, 120, 120, (uint16_t)17, { 17, 82, 8, 63, 0.00f, WatchFaceHandKind::Hour, 122, 122 }, { 16, 110, 8, 92, 0.00f, WatchFaceHandKind::Minute, 122, 122 }, { 11, 110, 5, 89, 0.00f, WatchFaceHandKind::Second, 123, 123 }, 0, { { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond } }, { 0, 0, 0, 0 }, { 120, 108, 120, 189, 65535, 0, 0, 1, 2, 6 }, { 0, 0, 0, 0, 0, 0 } },
  { "steampunk", "Steampunk", watch_face_assets::kBlob_steampunk, watch_face_assets::kBlobSize_steampunk, 120, 120, (uint16_t)3, { 14, 58, 7, 57, 0.00f, WatchFaceHandKind::Hour, -32768, -32768 }, { 10, 88, 5, 87, 0.00f, WatchFaceHandKind::Minute, -32768, -32768 }, { 6, 102, 3, 101, 0.00f, WatchFaceHandKind::Second, -32768, -32768 }, 0, { { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond } }, { 28, 28, 14, 14 }, { 0, 0, 120, 120, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } },
  { "wayfinder", "Wayfinder", watch_face_assets::kBlob_wayfinder, watch_face_assets::kBlobSize_wayfinder, 120, 120, (uint16_t)19, { 16, 58, 8, 57, 0.00f, WatchFaceHandKind::Hour, -32768, -32768 }, { 12, 88, 6, 87, 0.00f, WatchFaceHandKind::Minute, -32768, -32768 }, { 8, 106, 4, 105, 0.00f, WatchFaceHandKind::Second, -32768, -32768 }, 0, { { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, -32768, -32768 }, 0, 0, SubdialRole::ChronoSecond } }, { 24, 24, 12, 12 }, { 120, 108, 120, 132, 65535, 40147, 4228, 2, 1, 3 }, { 0, 0, 0, 0, 0, 0 } },
    };
    if (index >= asset_face_count()) {
        return nullptr;
    }
    return &kFaces[index];
}

#endif

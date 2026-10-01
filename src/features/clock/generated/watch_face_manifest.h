#ifndef WATCH_FACE_MANIFEST_H
#define WATCH_FACE_MANIFEST_H

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

enum class WatchFaceHandKind : uint8_t { Hour, Minute, Second, Chrono };

enum class SubdialRole : uint8_t { ChronoSecond = 0, ChronoMinute = 1, WallSecond = 2 };

struct WatchFaceHandMeta {
    uint16_t width;
    uint16_t height;
    int16_t pivot_x;
    int16_t pivot_y;
    float offset_deg;
    WatchFaceHandKind kind;
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
    bool has_second() const { return (flags & 0x0001) != 0; }
    bool has_hub() const { return (flags & 0x0002) != 0; }
    bool has_subdials() const { return (flags & 0x0004) != 0; }
    bool is_chronograph() const { return (flags & 0x0008) != 0; }
};

namespace watch_face_assets {
extern const uint8_t kBlob_aurora[];
extern const size_t kBlobSize_aurora;
extern const uint8_t kBlob_classic_chrono[];
extern const size_t kBlobSize_classic_chrono;
extern const uint8_t kBlob_demo[];
extern const size_t kBlobSize_demo;
extern const uint8_t kBlob_lake[];
extern const size_t kBlobSize_lake;
extern const uint8_t kBlob_steampunk[];
extern const size_t kBlobSize_steampunk;
}

inline size_t asset_face_count() { return 5; }

inline const AssetFaceMeta* asset_face_meta(size_t index) {
    static const AssetFaceMeta kFaces[] = {
  { "aurora", "Aurora", watch_face_assets::kBlob_aurora, watch_face_assets::kBlobSize_aurora, 120, 120, (uint16_t)3, { 14, 68, 7, 67, 0.00f, WatchFaceHandKind::Hour }, { 12, 96, 6, 95, 0.00f, WatchFaceHandKind::Minute }, { 6, 106, 3, 105, 0.00f, WatchFaceHandKind::Second }, 0, { { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond } }, { 24, 24, 12, 12 } },
  { "classic-chrono", "Classic Chrono", watch_face_assets::kBlob_classic_chrono, watch_face_assets::kBlobSize_classic_chrono, 120, 120, (uint16_t)14, { 12, 56, 6, 55, 0.00f, WatchFaceHandKind::Hour }, { 8, 84, 4, 83, 0.00f, WatchFaceHandKind::Minute }, { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Second }, 3, { { { 6, 40, 3, 39, 0.00f, WatchFaceHandKind::Chrono }, 52, 120, SubdialRole::WallSecond }, { { 6, 44, 3, 43, 0.00f, WatchFaceHandKind::Chrono }, 120, 52, SubdialRole::ChronoMinute }, { { 6, 48, 3, 47, 0.00f, WatchFaceHandKind::Chrono }, 120, 168, SubdialRole::ChronoSecond } }, { 20, 20, 10, 10 } },
  { "demo", "Demo Pack", watch_face_assets::kBlob_demo, watch_face_assets::kBlobSize_demo, 120, 120, (uint16_t)3, { 36, 60, 18, 59, 0.00f, WatchFaceHandKind::Hour }, { 24, 88, 12, 87, 0.00f, WatchFaceHandKind::Minute }, { 8, 96, 4, 95, 0.00f, WatchFaceHandKind::Second }, 0, { { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond } }, { 16, 16, 8, 8 } },
  { "lake", "Lake", watch_face_assets::kBlob_lake, watch_face_assets::kBlobSize_lake, 120, 120, (uint16_t)3, { 16, 62, 8, 61, 0.00f, WatchFaceHandKind::Hour }, { 10, 90, 5, 89, 0.00f, WatchFaceHandKind::Minute }, { 4, 102, 2, 101, 0.00f, WatchFaceHandKind::Second }, 0, { { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond } }, { 22, 22, 11, 11 } },
  { "steampunk", "Steampunk", watch_face_assets::kBlob_steampunk, watch_face_assets::kBlobSize_steampunk, 120, 120, (uint16_t)3, { 14, 58, 7, 57, 0.00f, WatchFaceHandKind::Hour }, { 10, 88, 5, 87, 0.00f, WatchFaceHandKind::Minute }, { 6, 102, 3, 101, 0.00f, WatchFaceHandKind::Second }, 0, { { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond }, { { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, SubdialRole::ChronoSecond } }, { 28, 28, 14, 14 } },
    };
    if (index >= asset_face_count()) {
        return nullptr;
    }
    return &kFaces[index];
}

#endif

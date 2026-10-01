#include "clock/clock_faces.h"

#include "clock/generated/watch_face_manifest.h"

extern const ClockFace* clock_face_midnight();

static const ClockFaceEntry kProcedural[] = {
    {"midnight", "Midnight", ClockFaceKind::Procedural, {.procedural = nullptr}},
};

static const ClockFaceEntry* build_registry(size_t& out_count) {
    static ClockFaceEntry entries[16];
    static size_t count = 0;
    static bool built = false;

    if (built) {
        out_count = count;
        return entries;
    }

    count = 0;
    for (size_t i = 0; i < sizeof(kProcedural) / sizeof(kProcedural[0]); ++i) {
        entries[count] = kProcedural[i];
        entries[count].procedural = clock_face_midnight();
        ++count;
    }

    for (size_t a = 0; a < asset_face_count(); ++a) {
        const AssetFaceMeta* meta = asset_face_meta(a);
        if (!meta) {
            continue;
        }
        entries[count].id = meta->id;
        entries[count].name = meta->name;
        entries[count].kind = ClockFaceKind::Asset;
        entries[count].asset_index = (uint16_t)a;
        ++count;
    }

    built = true;
    out_count = count;
    return entries;
}

int clock_face_count() {
    size_t n = 0;
    build_registry(n);
    return (int)n;
}

const ClockFaceEntry* clock_face_entry_at(int index) {
    size_t n = 0;
    const ClockFaceEntry* reg = build_registry(n);
    if (index < 0 || (size_t)index >= n) {
        return nullptr;
    }
    return &reg[index];
}

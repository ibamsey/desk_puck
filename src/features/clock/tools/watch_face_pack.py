#!/usr/bin/env python3
"""Pack src/features/clock/assets/faces/*/ into generated/ for firmware embed."""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("watch_face_pack: install Pillow (pip install pillow)", file=sys.stderr)
    sys.exit(1)

from _paths import ASSETS_FACES, GENERATED

ASSETS_DIR = ASSETS_FACES
OUT_DIR = GENERATED

CANVAS = 240
MAGIC = b"WPCK"
VERSION = 1
FLAG_HAS_SECOND = 0x0001
FLAG_HAS_HUB = 0x0002
FLAG_HAS_CHRONO = 0x0004
FLAG_IS_CHRONOGRAPH = 0x0008

HAND_MAX_W = 48
HAND_MAX_H = 110
HUB_MAX = 32
MAX_SUBDIALS = 3
SUBDIAL_ROLE = {
    "chrono_second": 0,
    "chrono_minute": 1,
    "wall_second": 2,
}


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def dial_to_rgb565(img: Image.Image) -> bytes:
    if img.size != (CANVAS, CANVAS):
        raise ValueError(f"dial must be {CANVAS}x{CANVAS}, got {img.size}")
    img = img.convert("RGB")
    out = bytearray(CANVAS * CANVAS * 2)
    i = 0
    for y in range(CANVAS):
        for x in range(CANVAS):
            r, g, b = img.getpixel((x, y))
            v = rgb565(r, g, b)
            out[i] = v & 0xFF
            out[i + 1] = (v >> 8) & 0xFF
            i += 2
    return bytes(out)


def sprite_to_rgb565a(img: Image.Image, name: str) -> tuple[int, int, int, int, bytes]:
    img = img.convert("RGBA")
    w, h = img.size
    if w > HAND_MAX_W or h > HAND_MAX_H:
        raise ValueError(f"{name}: sprite {w}x{h} exceeds max {HAND_MAX_W}x{HAND_MAX_H}")
    pivot_x = w // 2
    pivot_y = h - 1
    pixels = bytearray(w * h * 3)
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            v = rgb565(r, g, b)
            pixels[i] = v & 0xFF
            pixels[i + 1] = (v >> 8) & 0xFF
            pixels[i + 2] = a
            i += 3
    return w, h, pivot_x, pivot_y, bytes(pixels)


def pack_hand_chunk(w: int, h: int, px: int, py: int, pixels: bytes) -> bytes:
    hdr = struct.pack("<HHhhH", w, h, px, py, 0)
    return hdr + pixels


def collect_subdials(meta: dict, slug: str, behaviour: dict) -> list[dict]:
    if meta.get("subdials"):
        out: list[dict] = []
        for i, sd in enumerate(meta["subdials"]):
            if i >= MAX_SUBDIALS:
                raise ValueError(f"{slug}: at most {MAX_SUBDIALS} subdials")
            role = sd["role"]
            if role not in SUBDIAL_ROLE:
                raise ValueError(f"{slug}: unknown subdial role {role!r}")
            pos = sd.get("subdial", sd)
            out.append(
                {
                    "role": role,
                    "role_id": SUBDIAL_ROLE[role],
                    "file": sd["file"],
                    "x": int(pos["x"]),
                    "y": int(pos["y"]),
                    "offset_deg": float(sd.get("offset_deg", 0)),
                }
            )
        return out

    chrono_cfg = meta.get("chrono")
    if behaviour.get("chronograph") and chrono_cfg:
        sd = chrono_cfg.get("subdial", {"x": 120, "y": 168})
        return [
            {
                "role": "chrono_second",
                "role_id": SUBDIAL_ROLE["chrono_second"],
                "file": chrono_cfg["file"],
                "x": int(sd["x"]),
                "y": int(sd["y"]),
                "offset_deg": float(chrono_cfg.get("offset_deg", 0)),
            }
        ]
    if behaviour.get("chronograph"):
        raise ValueError(f"{slug}: chronograph requires subdials[] or chrono in face.json")
    return []


def load_face(folder: Path) -> dict:
    manifest_path = folder / "face.json"
    with manifest_path.open(encoding="utf-8") as f:
        meta = json.load(f)
    slug = folder.name
    if meta.get("id") != slug:
        raise ValueError(f"{slug}: face.json id must match folder name")
    pivot = meta.get("pivot", {"x": 120, "y": 120})
    hands_cfg = meta["hands"]

    dial_path = folder / "dial.png"
    dial_img = Image.open(dial_path)
    dial_bytes = dial_to_rgb565(dial_img)

    flags = 0
    chunks: list[bytes] = []
    hand_meta: dict = {}

    for key in ("hour", "minute"):
        hc = hands_cfg[key]
        path = folder / hc["file"]
        w, h, px, py, pix = sprite_to_rgb565a(Image.open(path), key)
        chunks.append(pack_hand_chunk(w, h, px, py, pix))
        hand_meta[key] = {
            "w": w,
            "h": h,
            "pivot_x": px,
            "pivot_y": py,
            "offset_deg": float(hc.get("offset_deg", 0)),
        }

    second_cfg = hands_cfg.get("second")
    if second_cfg and not second_cfg.get("optional", False):
        path = folder / second_cfg["file"]
        if path.exists():
            w, h, px, py, pix = sprite_to_rgb565a(Image.open(path), "second")
            chunks.append(pack_hand_chunk(w, h, px, py, pix))
            hand_meta["second"] = {
                "w": w,
                "h": h,
                "pivot_x": px,
                "pivot_y": py,
                "offset_deg": float(second_cfg.get("offset_deg", 0)),
            }
            flags |= FLAG_HAS_SECOND
    elif second_cfg and second_cfg.get("optional", True):
        path = folder / second_cfg.get("file", "second.png")
        if path.exists():
            w, h, px, py, pix = sprite_to_rgb565a(Image.open(path), "second")
            chunks.append(pack_hand_chunk(w, h, px, py, pix))
            hand_meta["second"] = {
                "w": w,
                "h": h,
                "pivot_x": px,
                "pivot_y": py,
                "offset_deg": float(second_cfg.get("offset_deg", 0)),
            }
            flags |= FLAG_HAS_SECOND

    behaviour = meta.get("behaviour") or {}
    subdial_specs = collect_subdials(meta, slug, behaviour)
    subdial_packed: list[dict] = []
    for sd in subdial_specs:
        path = folder / sd["file"]
        if not path.is_file():
            raise ValueError(f"{slug}: missing subdial {path.name}")
        w, h, px, py, pix = sprite_to_rgb565a(Image.open(path), sd["role"])
        chunks.append(pack_hand_chunk(w, h, px, py, pix))
        subdial_packed.append(
            {
                "w": w,
                "h": h,
                "pivot_x": px,
                "pivot_y": py,
                "offset_deg": sd["offset_deg"],
                "x": sd["x"],
                "y": sd["y"],
                "role_id": sd["role_id"],
            }
        )
    if subdial_packed:
        flags |= FLAG_HAS_CHRONO | FLAG_IS_CHRONOGRAPH

    hub_meta = None
    hub_cfg = meta.get("hub")
    if hub_cfg:
        path = folder / hub_cfg["file"]
        if path.exists():
            img = Image.open(path)
            w, h = img.size
            if w > HUB_MAX or h > HUB_MAX:
                raise ValueError(f"hub max {HUB_MAX}x{HUB_MAX}")
            px, py = w // 2, h // 2
            _, _, _, _, pix = sprite_to_rgb565a(img, "hub")
            chunks.append(pack_hand_chunk(w, h, px, py, pix))
            hub_meta = {"w": w, "h": h, "pivot_x": px, "pivot_y": py}
            flags |= FLAG_HAS_HUB

    header = struct.pack(
        "<4sHHhh",
        MAGIC,
        VERSION,
        flags,
        int(pivot["x"]),
        int(pivot["y"]),
    )
    blob = header + dial_bytes + b"".join(chunks)

    return {
        "id": slug,
        "name": meta.get("name", slug),
        "blob": blob,
        "flags": flags,
        "pivot_x": int(pivot["x"]),
        "pivot_y": int(pivot["y"]),
        "hands": hand_meta,
        "hub": hub_meta,
        "subdials": subdial_packed,
    }


def c_escape(data: bytes) -> str:
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        hexes = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"  {hexes},")
    return "\n".join(lines)


def hand_meta_cpp(h: dict, kind: str) -> str:
    return (
        f"{{ {h['w']}, {h['h']}, {h['pivot_x']}, {h['pivot_y']}, "
        f"{h['offset_deg']:.2f}f, WatchFaceHandKind::{kind} }}"
    )


def emit_cpp(faces: list[dict]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    assets_cpp = ['#include "clock/generated/watch_face_manifest.h"', "", "namespace watch_face_assets {", ""]
    manifest_entries = []

    for face in faces:
        sym = face["id"].replace("-", "_")
        blob = face["blob"]
        assets_cpp.append(f"alignas(4) const uint8_t kBlob_{sym}[] PROGMEM = {{")
        assets_cpp.append(c_escape(blob))
        assets_cpp.append("};")
        assets_cpp.append(f"const size_t kBlobSize_{sym} = {len(blob)};")
        assets_cpp.append("")

        h = face["hands"]
        hub = face["hub"]
        hour = h["hour"]
        minute = h["minute"]
        if "second" in h:
            second = hand_meta_cpp(h["second"], "Second")
        else:
            second = "{ 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Second }"
        if hub:
            hub_cpp = f"{{ {hub['w']}, {hub['h']}, {hub['pivot_x']}, {hub['pivot_y']} }}"
        else:
            hub_cpp = "{ 0, 0, 0, 0 }"
        sds = face.get("subdials") or []
        sub_count = len(sds)
        sub_entries = []
        role_enum = ["ChronoSecond", "ChronoMinute", "WallSecond"]
        for i in range(MAX_SUBDIALS):
            if i < sub_count:
                sd = sds[i]
                hand = hand_meta_cpp(sd, "Chrono")
                role = role_enum[sd["role_id"]]
                sub_entries.append(
                    f"{{ {hand}, {sd['x']}, {sd['y']}, SubdialRole::{role} }}"
                )
            else:
                sub_entries.append(
                    "{ { 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono }, 0, 0, "
                    "SubdialRole::ChronoSecond }"
                )
        sub_cpp = ", ".join(sub_entries)

        manifest_entries.append(
            f'  {{ "{face["id"]}", "{face["name"]}", '
            f"watch_face_assets::kBlob_{sym}, watch_face_assets::kBlobSize_{sym}, "
            f"{face['pivot_x']}, {face['pivot_y']}, "
            f"(uint16_t){face['flags']}, "
            f"{hand_meta_cpp(hour, 'Hour')}, "
            f"{hand_meta_cpp(minute, 'Minute')}, "
            f"{second}, "
            f"{sub_count}, "
            f"{{ {sub_cpp} }}, "
            f"{hub_cpp} }},"
        )

    assets_cpp.append("}  // namespace watch_face_assets")
    (OUT_DIR / "watch_face_assets.cpp").write_text("\n".join(assets_cpp) + "\n", encoding="utf-8")

    externs = "\n".join(
        f"extern const uint8_t kBlob_{f['id'].replace('-', '_')}[];\n"
        f"extern const size_t kBlobSize_{f['id'].replace('-', '_')};"
        for f in faces
    )
    manifest_h = f"""#ifndef WATCH_FACE_MANIFEST_H
#define WATCH_FACE_MANIFEST_H

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

enum class WatchFaceHandKind : uint8_t {{ Hour, Minute, Second, Chrono }};

enum class SubdialRole : uint8_t {{ ChronoSecond = 0, ChronoMinute = 1, WallSecond = 2 }};

struct WatchFaceHandMeta {{
    uint16_t width;
    uint16_t height;
    int16_t pivot_x;
    int16_t pivot_y;
    float offset_deg;
    WatchFaceHandKind kind;
}};

struct WatchFaceHubMeta {{
    uint16_t width;
    uint16_t height;
    int16_t pivot_x;
    int16_t pivot_y;
}};

struct SubdialMeta {{
    WatchFaceHandMeta hand;
    int16_t x;
    int16_t y;
    SubdialRole role;
}};

static constexpr uint8_t kMaxSubdials = 3;

struct AssetFaceMeta {{
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
    bool has_second() const {{ return (flags & 0x0001) != 0; }}
    bool has_hub() const {{ return (flags & 0x0002) != 0; }}
    bool has_subdials() const {{ return (flags & 0x0004) != 0; }}
    bool is_chronograph() const {{ return (flags & 0x0008) != 0; }}
}};

namespace watch_face_assets {{
{externs}
}}

inline size_t asset_face_count() {{ return {len(faces)}; }}

inline const AssetFaceMeta* asset_face_meta(size_t index) {{
    static const AssetFaceMeta kFaces[] = {{
{chr(10).join(manifest_entries)}
    }};
    if (index >= asset_face_count()) {{
        return nullptr;
    }}
    return &kFaces[index];
}}

#endif
"""
    (OUT_DIR / "watch_face_manifest.h").write_text(manifest_h, encoding="utf-8")

    empty_h = """#ifndef WATCH_FACE_REGISTRY_H
#define WATCH_FACE_REGISTRY_H
#include "clock/generated/watch_face_manifest.h"
#endif
"""
    (OUT_DIR / "watch_face_registry.h").write_text(empty_h, encoding="utf-8")

    total = sum(len(f["blob"]) for f in faces)
    print(f"watch_face_pack: {len(faces)} face(s), {total} bytes flash")


def ensure_demo_assets() -> None:
    demo = ASSETS_DIR / "demo"
    demo.mkdir(parents=True, exist_ok=True)
    manifest = demo / "face.json"
    if not manifest.exists():
        manifest.write_text(
            json.dumps(
                {
                    "id": "demo",
                    "name": "Demo Pack",
                    "version": 1,
                    "pivot": {"x": 120, "y": 120},
                    "hands": {
                        "hour": {"file": "hour.png", "length_px": 52, "offset_deg": 0},
                        "minute": {"file": "minute.png", "length_px": 78, "offset_deg": 0},
                        "second": {"file": "second.png", "length_px": 88, "offset_deg": 0, "optional": True},
                    },
                    "hub": {"file": "hub.png"},
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )

    def save_hand(path: Path, w: int, h: int, color: tuple[int, int, int, int]) -> None:
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        cx = w // 2
        for y in range(h):
            for x in range(w):
                if abs(x - cx) <= 2 and y < h - 4:
                    img.putpixel((x, y), color)
        path.parent.mkdir(parents=True, exist_ok=True)
        img.save(path)

    dial_path = demo / "dial.png"
    if not dial_path.exists():
        dial = Image.new("RGB", (CANVAS, CANVAS), (20, 40, 80))
        for y in range(CANVAS):
            for x in range(CANVAS):
                dx, dy = x - 120, y - 120
                if dx * dx + dy * dy < 100 * 100:
                    dial.putpixel((x, y), (30 + y // 8, 60, 120))
        dial.save(dial_path)

    if not (demo / "hour.png").exists():
        save_hand(demo / "hour.png", 36, 60, (200, 200, 220, 255))
    if not (demo / "minute.png").exists():
        save_hand(demo / "minute.png", 24, 88, (255, 255, 255, 255))
    if not (demo / "second.png").exists():
        save_hand(demo / "second.png", 8, 96, (0, 255, 255, 255))
    if not (demo / "hub.png").exists():
        hub = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        for y in range(16):
            for x in range(16):
                if (x - 8) ** 2 + (y - 8) ** 2 <= 49:
                    hub.putpixel((x, y), (180, 180, 200, 255))
        hub.save(demo / "hub.png")


def main() -> int:
    ensure_demo_assets()
    if not ASSETS_DIR.is_dir():
        print("No clock assets/faces directory", file=sys.stderr)
        return 1

    faces: list[dict] = []
    for folder in sorted(ASSETS_DIR.iterdir()):
        if not folder.is_dir() or folder.name.startswith("_"):
            continue
        if not (folder / "face.json").is_file():
            continue
        print(f"Packing {folder.name}...")
        faces.append(load_face(folder))

    if not faces:
        print("No watch faces to pack; writing empty manifest")
        OUT_DIR.mkdir(parents=True, exist_ok=True)
        (OUT_DIR / "watch_face_manifest.h").write_text(
            """#ifndef WATCH_FACE_MANIFEST_H
#define WATCH_FACE_MANIFEST_H
#include <cstddef>
inline size_t asset_face_count() { return 0; }
struct AssetFaceMeta;
inline const AssetFaceMeta* asset_face_meta(size_t) { return nullptr; }
#endif
""",
            encoding="utf-8",
        )
        (OUT_DIR / "watch_face_assets.cpp").write_text(
            '#include "clock/generated/watch_face_manifest.h"\n', encoding="utf-8"
        )
        return 0

    emit_cpp(faces)
    return 0


if __name__ == "__main__":
    sys.exit(main())

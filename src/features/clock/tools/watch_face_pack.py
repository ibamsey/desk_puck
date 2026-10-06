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
FLAG_HAS_DIGITAL = 0x0010
FLAG_HAS_WEATHER = 0x0020

WEATHER_ICON_MAX = 64

HAND_MAX_W = 48
HAND_MAX_H = 110
HUB_MAX = 32
DIAL_PIVOT_USE_FACE = -32768
MAX_SUBDIALS = 3
SUBDIAL_ROLE = {
    "chrono_second": 0,
    "chrono_minute": 1,
    "wall_second": 2,
}

DEFAULT_DIGITAL_READOUT = {
    "time_x": 120,
    "time_y": 108,
    "date_x": 120,
    "date_y": 132,
    "time_color": 0xFFFF,
    "date_color": 0x9CD3,
    "bg_color": 0x1084,
    "time_text_size": 2,
    "date_text_size": 1,
    "flags": 0x03,  # show time + date, short date format
}

DIGITAL_FLAG_SHOW_TIME = 0x01
DIGITAL_FLAG_SHOW_DATE = 0x02
DIGITAL_DATE_FORMAT_DAY_DD = 0x04
DIGITAL_DATE_FORMAT_DAY_D = 0x08
DIGITAL_TIME_FORMAT_12H = 0x10
DIGITAL_DATE_FORMAT_WEEKDAY_MONTH_DAY = 0x20
DIGITAL_DATE_FORMAT_SPLIT_WEEKDAY_DAY = 0x40


def digital_flags_from_json(raw: dict) -> int:
    flags = int(raw.get("flags", DEFAULT_DIGITAL_READOUT["flags"]))
    if "show_time" in raw:
        if raw["show_time"]:
            flags |= DIGITAL_FLAG_SHOW_TIME
        else:
            flags &= ~DIGITAL_FLAG_SHOW_TIME
    if "show_date" in raw:
        if raw["show_date"]:
            flags |= DIGITAL_FLAG_SHOW_DATE
        else:
            flags &= ~DIGITAL_FLAG_SHOW_DATE
    fmt = raw.get("date_format")
    if fmt:
        flags &= ~(
            DIGITAL_DATE_FORMAT_DAY_DD
            | DIGITAL_DATE_FORMAT_DAY_D
            | DIGITAL_DATE_FORMAT_WEEKDAY_MONTH_DAY
            | DIGITAL_DATE_FORMAT_SPLIT_WEEKDAY_DAY
        )
        if fmt == "day":
            flags |= DIGITAL_DATE_FORMAT_DAY_DD
        elif fmt == "day_unpadded":
            flags |= DIGITAL_DATE_FORMAT_DAY_D
        elif fmt == "weekday_month_day":
            flags |= DIGITAL_DATE_FORMAT_WEEKDAY_MONTH_DAY
        elif fmt == "split_weekday_day":
            flags |= DIGITAL_DATE_FORMAT_SPLIT_WEEKDAY_DAY
    if raw.get("time_format") == "12h_ampm":
        flags |= DIGITAL_TIME_FORMAT_12H
    return flags & 0xFF


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


def sprite_to_rgb565a(
    img: Image.Image,
    name: str,
    pivot_x: int | None = None,
    pivot_y: int | None = None,
    max_w: int = HAND_MAX_W,
    max_h: int = HAND_MAX_H,
) -> tuple[int, int, int, int, bytes]:
    img = img.convert("RGBA")
    w, h = img.size
    if w > max_w or h > max_h:
        raise ValueError(f"{name}: sprite {w}x{h} exceeds max {max_w}x{max_h}")
    if pivot_x is None:
        pivot_x = w // 2
    if pivot_y is None:
        pivot_y = h - 1
    pivot_x = max(0, min(w - 1, int(pivot_x)))
    pivot_y = max(0, min(h - 1, int(pivot_y)))
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


def parse_rgb565(value: object) -> int:
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def collect_digital_readout(behaviour: dict) -> tuple[bool, dict | None]:
    raw = behaviour.get("digital_readout")
    if not raw:
        return False, None
    cfg = dict(DEFAULT_DIGITAL_READOUT)
    if isinstance(raw, dict):
        for key in cfg:
            if key not in raw:
                continue
            if "color" in key:
                cfg[key] = parse_rgb565(raw[key])
            else:
                cfg[key] = int(raw[key])
    elif raw is not True:
        raise ValueError("behaviour.digital_readout must be true or an object with layout/colours")
    if isinstance(raw, dict):
        cfg["flags"] = digital_flags_from_json(raw)
    return True, cfg


def digital_meta_cpp(cfg: dict | None) -> str:
    if not cfg:
        return "{ 0, 0, 120, 120, 0, 0, 0, 0, 0, 0 }"
    return (
        f"{{ {cfg['time_x']}, {cfg['time_y']}, {cfg['date_x']}, {cfg['date_y']}, "
        f"{cfg['time_color']}, {cfg['date_color']}, {cfg['bg_color']}, "
        f"{cfg['time_text_size']}, {cfg['date_text_size']}, {cfg['flags']} }}"
    )


def hand_dial_pivot(hc: dict) -> tuple[int, int]:
    anchor = hc.get("anchor")
    if isinstance(anchor, dict) and "x" in anchor and "y" in anchor:
        return int(anchor["x"]), int(anchor["y"])
    return DIAL_PIVOT_USE_FACE, DIAL_PIVOT_USE_FACE


def hand_sprite_pivot(hc: dict) -> tuple[int | None, int | None]:
    hp = hc.get("hand_pivot")
    if isinstance(hp, dict) and "x" in hp and "y" in hp:
        return int(hp["x"]), int(hp["y"])
    return None, None


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
            hp = sd.get("hand_pivot") or {}
            out.append(
                {
                    "role": role,
                    "role_id": SUBDIAL_ROLE[role],
                    "file": sd["file"],
                    "x": int(pos["x"]),
                    "y": int(pos["y"]),
                    "offset_deg": float(sd.get("offset_deg", 0)),
                    "hand_pivot_x": int(hp["x"]) if "x" in hp else None,
                    "hand_pivot_y": int(hp["y"]) if "y" in hp else None,
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
        spx, spy = hand_sprite_pivot(hc)
        w, h, px, py, pix = sprite_to_rgb565a(Image.open(path), key, pivot_x=spx, pivot_y=spy)
        chunks.append(pack_hand_chunk(w, h, px, py, pix))
        dpx, dpy = hand_dial_pivot(hc)
        hand_meta[key] = {
            "w": w,
            "h": h,
            "pivot_x": px,
            "pivot_y": py,
            "offset_deg": float(hc.get("offset_deg", 0)),
            "dial_pivot_x": dpx,
            "dial_pivot_y": dpy,
        }

    second_cfg = hands_cfg.get("second")
    if second_cfg and not second_cfg.get("optional", False):
        path = folder / second_cfg["file"]
        if path.exists():
            spx, spy = hand_sprite_pivot(second_cfg)
            w, h, px, py, pix = sprite_to_rgb565a(
                Image.open(path), "second", pivot_x=spx, pivot_y=spy
            )
            chunks.append(pack_hand_chunk(w, h, px, py, pix))
            dpx, dpy = hand_dial_pivot(second_cfg)
            hand_meta["second"] = {
                "w": w,
                "h": h,
                "pivot_x": px,
                "pivot_y": py,
                "offset_deg": float(second_cfg.get("offset_deg", 0)),
                "dial_pivot_x": dpx,
                "dial_pivot_y": dpy,
            }
            flags |= FLAG_HAS_SECOND
    elif second_cfg and second_cfg.get("optional", True):
        path = folder / second_cfg.get("file", "second.png")
        if path.exists():
            spx, spy = hand_sprite_pivot(second_cfg)
            w, h, px, py, pix = sprite_to_rgb565a(
                Image.open(path), "second", pivot_x=spx, pivot_y=spy
            )
            chunks.append(pack_hand_chunk(w, h, px, py, pix))
            dpx, dpy = hand_dial_pivot(second_cfg)
            hand_meta["second"] = {
                "w": w,
                "h": h,
                "pivot_x": px,
                "pivot_y": py,
                "offset_deg": float(second_cfg.get("offset_deg", 0)),
                "dial_pivot_x": dpx,
                "dial_pivot_y": dpy,
            }
            flags |= FLAG_HAS_SECOND

    behaviour = meta.get("behaviour") or {}
    subdial_specs = collect_subdials(meta, slug, behaviour)
    subdial_packed: list[dict] = []
    for sd in subdial_specs:
        path = folder / sd["file"]
        if not path.is_file():
            raise ValueError(f"{slug}: missing subdial {path.name}")
        hp_x = sd.get("hand_pivot_x")
        hp_y = sd.get("hand_pivot_y")
        w, h, px, py, pix = sprite_to_rgb565a(
            Image.open(path),
            sd["role"],
            pivot_x=hp_x,
            pivot_y=hp_y,
        )
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

    has_digital, digital_cfg = collect_digital_readout(behaviour)
    if has_digital:
        flags |= FLAG_HAS_DIGITAL

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

    weather_meta = None
    weather_cfg = meta.get("weather_icon")
    if weather_cfg:
        wpath = folder / weather_cfg["file"]
        if not wpath.is_file():
            raise ValueError(f"{slug}: missing {weather_cfg['file']}")
        wimg = Image.open(wpath).convert("RGBA")
        target = int(weather_cfg.get("display_size", WEATHER_ICON_MAX))
        if max(wimg.size) != target:
            wimg = wimg.resize((target, target), Image.Resampling.LANCZOS)
        w, h = wimg.size
        if w > WEATHER_ICON_MAX or h > WEATHER_ICON_MAX:
            raise ValueError(f"{slug}: weather icon sheet max {WEATHER_ICON_MAX}px after scale")
        px, py = w // 2, h // 2
        _, _, _, _, pix = sprite_to_rgb565a(
            wimg, "weather", pivot_x=px, pivot_y=py, max_w=WEATHER_ICON_MAX, max_h=WEATHER_ICON_MAX
        )
        chunks.append(pack_hand_chunk(w, h, px, py, pix))
        weather_meta = {
            "x": int(weather_cfg["x"]),
            "y": int(weather_cfg["y"]),
            "grid_cols": int(weather_cfg.get("grid_cols", 3)),
            "grid_rows": int(weather_cfg.get("grid_rows", 3)),
            "w": w,
            "h": h,
        }
        flags |= FLAG_HAS_WEATHER

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
        "digital": digital_cfg,
        "weather": weather_meta,
    }


def c_escape(data: bytes) -> str:
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        hexes = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"  {hexes},")
    return "\n".join(lines)


def hand_meta_cpp(h: dict, kind: str) -> str:
    dpx = h.get("dial_pivot_x", DIAL_PIVOT_USE_FACE)
    dpy = h.get("dial_pivot_y", DIAL_PIVOT_USE_FACE)
    return (
        f"{{ {h['w']}, {h['h']}, {h['pivot_x']}, {h['pivot_y']}, "
        f"{h['offset_deg']:.2f}f, WatchFaceHandKind::{kind}, {dpx}, {dpy} }}"
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
            second = (
                f"{{ 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Second, "
                f"{DIAL_PIVOT_USE_FACE}, {DIAL_PIVOT_USE_FACE} }}"
            )
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
                    f"{{ {{ 0, 0, 0, 0, 0.0f, WatchFaceHandKind::Chrono, "
                    f"{DIAL_PIVOT_USE_FACE}, {DIAL_PIVOT_USE_FACE} }}, 0, 0, "
                    "SubdialRole::ChronoSecond }"
                )
        sub_cpp = ", ".join(sub_entries)

        digital_cpp = digital_meta_cpp(face.get("digital"))
        weather = face.get("weather")
        if weather:
            weather_cpp = (
                f"{{ {weather['x']}, {weather['y']}, {weather['grid_cols']}, "
                f"{weather['grid_rows']}, {weather['w']}, {weather['h']} }}"
            )
        else:
            weather_cpp = "{ 0, 0, 0, 0, 0, 0 }"
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
            f"{hub_cpp}, "
            f"{digital_cpp}, "
            f"{weather_cpp} }},"
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

static constexpr int16_t kDialPivotUseFace = {DIAL_PIVOT_USE_FACE};

struct WatchFaceHandMeta {{
    uint16_t width;
    uint16_t height;
    int16_t pivot_x;
    int16_t pivot_y;
    float offset_deg;
    WatchFaceHandKind kind;
    /** Screen pivot for rotation; kDialPivotUseFace = use face pivot from AssetFaceMeta. */
    int16_t dial_pivot_x;
    int16_t dial_pivot_y;
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

struct AssetDigitalReadoutMeta {{
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
}};

struct AssetWeatherIconMeta {{
    int16_t x;
    int16_t y;
    uint8_t grid_cols;
    uint8_t grid_rows;
    uint16_t sheet_w;
    uint16_t sheet_h;
}};

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
    AssetDigitalReadoutMeta digital;
    AssetWeatherIconMeta weather;
    bool has_second() const {{ return (flags & 0x0001) != 0; }}
    bool has_hub() const {{ return (flags & 0x0002) != 0; }}
    bool has_subdials() const {{ return (flags & 0x0004) != 0; }}
    bool is_chronograph() const {{ return (flags & 0x0008) != 0; }}
    bool has_digital_readout() const {{ return (flags & 0x0010) != 0; }}
    bool has_weather_icon() const {{ return (flags & 0x0020) != 0; }}
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


def main() -> int:
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

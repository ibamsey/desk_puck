#!/usr/bin/env python3
"""Aurora: dial/hour/hub from source JPGs; minute/second drawn to match hour (JPG extract is unreliable)."""

from __future__ import annotations

import json
import math
import sys
from collections import deque
from pathlib import Path

from PIL import Image

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))
from _paths import face_dir  # noqa: E402

FACE = face_dir("aurora")
SRC = FACE / "source"
OUT = FACE

CANVAS = 240

# Faceted gold + cyan second (match shipped hour.png style)
GOLD_DARK = (92, 68, 28, 255)
GOLD_MID = (168, 132, 52, 255)
GOLD_HI = (238, 210, 120, 255)
CYAN_SEC = (40, 210, 220, 255)
CYAN_HI = (160, 245, 255, 255)


def draw_taper_hand(
    width: int,
    height: int,
    tip_half: int,
    base_half: int,
    colors: tuple[tuple[int, int, int, int], tuple[int, int, int, int], tuple[int, int, int, int]],
) -> Image.Image:
    dark, mid, hi = colors
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    px = img.load()
    cx = width // 2
    for y in range(height):
        t = y / max(height - 1, 1)
        half = max(tip_half, int(round(tip_half + (base_half - tip_half) * t)))
        for x in range(width):
            if abs(x - cx) > half:
                continue
            edge = half > 0 and (abs(x - cx) == half or y == 0)
            center = abs(x - cx) <= max(0, half - 1)
            if edge:
                px[x, y] = dark
            elif center and t > 0.35:
                px[x, y] = hi if (x + y) % 2 == 0 else mid
            else:
                px[x, y] = mid
    return img


def src_raw(stem: str) -> Path:
    """Resolve dial_raw.jpg or legacy aurora_dial_raw.jpg in source/."""
    for name in (f"{stem}_raw.jpg", f"{stem}_raw.png", f"aurora_{stem}_raw.jpg", f"aurora_{stem}_raw.png"):
        path = SRC / name
        if path.is_file():
            return path
    raise FileNotFoundError(f"No source for {stem} under {SRC}")


def load_rgba(path: Path) -> Image.Image:
    return Image.open(path).convert("RGBA")


def dial_from_raw(path: Path) -> Image.Image:
    img = load_rgba(path)
    img = img.resize((CANVAS, CANVAS), Image.Resampling.LANCZOS)
    px = img.load()
    cx, cy, r = CANVAS // 2, CANVAS // 2, CANVAS // 2 - 2
    for y in range(CANVAS):
        for x in range(CANVAS):
            if (x - cx) ** 2 + (y - cy) ** 2 > r * r:
                px[x, y] = (0, 0, 0, 255)
    return strip_baked_hands_from_dial(img.convert("RGB"))


def strip_baked_hands_from_dial(img: Image.Image) -> Image.Image:
    """Remove painted hands from AI dial art (sprites draw live time)."""
    img = img.convert("RGB")
    px = img.load()
    out = img.copy()
    op = out.load()
    cx, cy = CANVAS // 2, CANVAS // 2
    sample_r = 108
    for y in range(CANVAS):
        for x in range(CANVAS):
            dx, dy = x - cx, y - cy
            dist = math.hypot(dx, dy)
            if dist < 16 or dist > 94:
                continue
            angle = math.atan2(dy, dx)
            sx = int(round(cx + math.cos(angle) * sample_r))
            sy = int(round(cy + math.sin(angle) * sample_r))
            if 0 <= sx < CANVAS and 0 <= sy < CANVAS:
                op[x, y] = px[sx, sy]
    return out


def is_background_pixel(r: int, g: int, b: int) -> bool:
    if max(r, g, b) - min(r, g, b) > 24:
        return False
    if r >= 250 and g >= 250 and b >= 250:
        return True
    if r <= 12 and g <= 12 and b <= 12:
        return True
    if abs(r - g) <= 12 and abs(g - b) <= 12:
        if r >= 170:
            return True
        if 105 <= r <= 155:
            return True
    return False


def flood_background_transparent(img: Image.Image) -> Image.Image:
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    seen = [[False] * w for _ in range(h)]
    q: deque[tuple[int, int]] = deque()

    def push(x: int, y: int) -> None:
        if x < 0 or y < 0 or x >= w or y >= h or seen[y][x]:
            return
        r, g, b, _a = px[x, y]
        if not is_background_pixel(r, g, b):
            return
        seen[y][x] = True
        q.append((x, y))

    for x in range(w):
        push(x, 0)
        push(x, h - 1)
    for y in range(h):
        push(0, y)
        push(w - 1, y)

    while q:
        x, y = q.popleft()
        r, g, b, _a = px[x, y]
        px[x, y] = (r, g, b, 0)
        push(x + 1, y)
        push(x - 1, y)
        push(x, y + 1)
        push(x, y - 1)

    return img


def solidify_alpha(img: Image.Image, threshold: int = 24) -> Image.Image:
    img = img.convert("RGBA")
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            if a > threshold:
                px[x, y] = (r, g, b, 255)
            else:
                px[x, y] = (0, 0, 0, 0)
    return img


def dilate_rgba(img: Image.Image, radius: int) -> Image.Image:
    if radius <= 0:
        return img
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    op = out.load()
    for y in range(h):
        for x in range(w):
            best = None
            for dy in range(-radius, radius + 1):
                for dx in range(-radius, radius + 1):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        r, g, b, a = px[nx, ny]
                        if a > 32 and (best is None or a > best[3]):
                            best = (r, g, b, a)
            if best:
                op[x, y] = (best[0], best[1], best[2], 255)
    return out


def extract_hand_sprite(
    img: Image.Image,
    *,
    max_w: int,
    max_h: int,
    slice_w: int,
    min_row_pixels: int,
    dilate: int = 0,
) -> Image.Image:
    img = flood_background_transparent(img)
    w, h = img.size
    px = img.load()

    total = 0
    sum_x = 0
    for y in range(h):
        for x in range(w):
            if px[x, y][3] >= 128:
                total += 1
                sum_x += x
    if total == 0:
        return Image.new("RGBA", (8, 8), (0, 0, 0, 0))

    cx = sum_x / total
    x0 = max(0, int(cx - slice_w / 2))
    x1 = min(w, int(cx + slice_w / 2))

    rows = [
        y
        for y in range(h)
        if sum(1 for x in range(x0, x1) if px[x, y][3] >= 128) >= min_row_pixels
    ]
    if not rows:
        return Image.new("RGBA", (8, 8), (0, 0, 0, 0))

    y0, y1 = rows[0], rows[-1]
    crop = img.crop((x0, y0, x1, y1 + 1))
    crop = dilate_rgba(crop, dilate)
    crop = solidify_alpha(crop)
    tight = crop.getbbox()
    if tight:
        crop = crop.crop(tight)

    cw, ch = crop.size
    target_h = max_h - 1
    scale = min(max_w / cw, target_h / ch, 1.0)
    nw = max(8, min(max_w, int(round(cw * scale))))
    nh = max(1, min(target_h, int(round(ch * scale))))
    if (nw, nh) != (cw, ch):
        crop = crop.resize((nw, nh), Image.Resampling.LANCZOS)
        crop = solidify_alpha(crop, 16)
        cw, ch = crop.size

    out_w = min(max(cw, 8), max_w)
    out_h = ch + 1
    out = Image.new("RGBA", (out_w, out_h), (0, 0, 0, 0))
    paste_x = out_w // 2 - cw // 2
    out.paste(crop, (paste_x, 0), crop)
    return out


def hub_from_raw(path: Path) -> Image.Image:
    img = flood_background_transparent(load_rgba(path))
    bbox = img.getbbox()
    if bbox:
        img = img.crop(bbox)
    img = img.resize((24, 24), Image.Resampling.LANCZOS)
    out = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    out.paste(img, ((24 - img.width) // 2, (24 - img.height) // 2), img)
    return out


def has_source_raws() -> bool:
    try:
        src_raw("dial")
        src_raw("hub")
        return True
    except FileNotFoundError:
        return False


def procedural_dial() -> Image.Image:
    """Placeholder when source/ JPGs are missing (restore via raws + re-run prep)."""
    img = Image.new("RGB", (CANVAS, CANVAS), (4, 8, 28))
    px = img.load()
    cx, cy = CANVAS // 2, CANVAS // 2
    for y in range(CANVAS):
        for x in range(CANVAS):
            dx, dy = x - cx, y - cy
            r = math.hypot(dx, dy)
            if r > 118:
                continue
            t = r / 118.0
            base = (int(8 + t * 18), int(12 + t * 24), int(40 + t * 50))
            wave = math.sin(x * 0.04 + y * 0.02) * 0.5 + math.sin(y * 0.05) * 0.5
            aur = max(0.0, wave + 0.15 - t * 0.35)
            g = int(base[1] + aur * 120)
            b = int(base[2] + aur * 80)
            te = int(aur * 90)
            px[x, y] = (
                min(255, base[0] + te // 3),
                min(255, g),
                min(255, b),
            )
    for i in range(24):
        ang = math.radians(i * 15 - 90)
        x0 = int(cx + math.cos(ang) * 100)
        y0 = int(cy + math.sin(ang) * 100)
        x1 = int(cx + math.cos(ang) * 112)
        y1 = int(cy + math.sin(ang) * 112)
        for t in range(13):
            x = x0 + (x1 - x0) * t // 12
            y = y0 + (y1 - y0) * t // 12
            if 0 <= x < CANVAS and 0 <= y < CANVAS:
                px[x, y] = (180, 200, 220)
    return img


def procedural_hub() -> Image.Image:
    out = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    px = out.load()
    for y in range(24):
        for x in range(24):
            d2 = (x - 12) ** 2 + (y - 12) ** 2
            if d2 <= 64:
                px[x, y] = GOLD_MID if d2 <= 36 else GOLD_DARK
    return out


def write_hands_and_manifest() -> None:
    draw_taper_hand(14, 68, tip_half=2, base_half=6, colors=(GOLD_DARK, GOLD_MID, GOLD_HI)).save(
        OUT / "hour.png"
    )
    draw_taper_hand(12, 96, tip_half=1, base_half=5, colors=(GOLD_DARK, GOLD_MID, GOLD_HI)).save(
        OUT / "minute.png"
    )
    draw_taper_hand(
        6, 106, tip_half=0, base_half=2, colors=((20, 120, 130, 255), CYAN_SEC, CYAN_HI)
    ).save(OUT / "second.png")
    manifest = {
        "id": "aurora",
        "name": "Aurora",
        "version": 1,
        "pivot": {"x": 120, "y": 120},
        "hands": {
            "hour": {"file": "hour.png", "length_px": 52, "offset_deg": 0},
            "minute": {"file": "minute.png", "length_px": 78, "offset_deg": 0},
            "second": {
                "file": "second.png",
                "length_px": 88,
                "offset_deg": 0,
                "optional": True,
            },
        },
        "hub": {"file": "hub.png"},
    }
    (OUT / "face.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    SRC.mkdir(parents=True, exist_ok=True)

    if has_source_raws():
        dial_from_raw(src_raw("dial")).save(OUT / "dial.png")
        hub_from_raw(src_raw("hub")).save(OUT / "hub.png")
    else:
        print(f"No aurora source JPGs in {SRC}; writing procedural dial/hub (drop raws to restore AI art)")
        procedural_dial().save(OUT / "dial.png")
        procedural_hub().save(OUT / "hub.png")

    write_hands_and_manifest()

    print(f"Prepared {OUT}")


if __name__ == "__main__":
    main()

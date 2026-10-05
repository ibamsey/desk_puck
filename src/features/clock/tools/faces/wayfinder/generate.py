#!/usr/bin/env python3
"""Generate Wayfinder tactical watch face PNGs (240×240, high-detail procedural art)."""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))
from _paths import face_dir  # noqa: E402

OUT = face_dir("wayfinder")
SCALE = 2
CANVAS = 240
CX, CY = 120, 120

# Tactical palette (safety orange + high-contrast mono)
BLACK = (8, 8, 10)
BG_MID = (22, 24, 28)
GRID = (38, 42, 48)
WHITE = (245, 245, 248)
GRAY = (140, 144, 152)
ORANGE = (255, 95, 20)
ORANGE_DIM = (180, 68, 16)
BLUE_RING = (28, 52, 88)


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def load_font(size: int, bold: bool = True) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        Path("C:/Windows/Fonts/arialbd.ttf"),
        Path("C:/Windows/Fonts/segoeuib.ttf"),
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"),
    ]
    for path in candidates:
        if path.is_file():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def downscale(img: Image.Image) -> Image.Image:
    return img.resize((CANVAS, CANVAS), Image.Resampling.LANCZOS)


def radial_grid(draw: ImageDraw.ImageDraw, cx: float, cy: float, max_r: float) -> None:
    for r in range(12, int(max_r), 14):
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), outline=GRID, width=1)
    for deg in range(0, 360, 15):
        rad = math.radians(deg - 90)
        x1 = cx + math.cos(rad) * 18
        y1 = cy + math.sin(rad) * 18
        x2 = cx + math.cos(rad) * (max_r - 4)
        y2 = cy + math.sin(rad) * (max_r - 4)
        draw.line((x1, y1, x2, y2), fill=GRID, width=1)


def make_dial() -> Image.Image:
    w = CANVAS * SCALE
    img = Image.new("RGB", (w, w), BLACK)
    px = img.load()
    cx = cy = w // 2
    max_r = w // 2 - 2

    for y in range(w):
        for x in range(w):
            dist = math.hypot(x - cx, y - cy)
            if dist > max_r:
                px[x, y] = (0, 0, 0)
                continue
            t = dist / max_r
            base = (
                int(lerp(BG_MID[0], BLACK[0], t * 0.7)),
                int(lerp(BG_MID[1], BLACK[1], t * 0.7)),
                int(lerp(BG_MID[2], BLACK[2], t * 0.7)),
            )
            px[x, y] = base

    draw = ImageDraw.Draw(img)
    radial_grid(draw, cx, cy, max_r - 8 * SCALE)

    # Compass / degree outer ring
    outer = max_r - 4 * SCALE
    draw.ellipse((cx - outer, cy - outer, cx + outer, cy + outer), outline=BLUE_RING, width=2 * SCALE)
    inner_comp = outer - 6 * SCALE
    draw.ellipse((cx - inner_comp, cy - inner_comp, cx + inner_comp, cy + inner_comp), outline=GRID, width=1)

    for deg in range(0, 360, 2):
        rad = math.radians(deg - 90)
        major = deg % 30 == 0
        minor = deg % 10 == 0
        if not major and not minor:
            continue
        r0 = inner_comp - (4 * SCALE if major else 2 * SCALE)
        r1 = inner_comp
        x0 = cx + math.cos(rad) * r0
        y0 = cy + math.sin(rad) * r0
        x1 = cx + math.cos(rad) * r1
        y1 = cy + math.sin(rad) * r1
        col = WHITE if major else GRAY
        draw.line((x0, y0, x1, y1), fill=col, width=(2 * SCALE if major else 1))

    font_card = load_font(14 * SCALE, bold=True)
    for label, deg in (("N", 0), ("E", 90), ("S", 180), ("W", 270)):
        rad = math.radians(deg - 90)
        tx = cx + math.cos(rad) * (outer - 14 * SCALE)
        ty = cy + math.sin(rad) * (outer - 14 * SCALE)
        draw.text((tx, ty), label, fill=WHITE, font=font_card, anchor="mm")

    # Segmented activity arcs (decorative, static)
    for start, end in ((200, 250), (290, 340), (20, 70)):
        draw.arc(
            (cx - 92 * SCALE, cy - 92 * SCALE, cx + 92 * SCALE, cy + 92 * SCALE),
            start,
            end,
            fill=WHITE,
            width=3 * SCALE,
        )

    # Hour numerals — bold block style
    font_hour = load_font(22 * SCALE, bold=True)
    font_hour_12 = load_font(28 * SCALE, bold=True)
    for hour in range(1, 13):
        ang = math.radians(hour * 30 - 90)
        r = 68 * SCALE
        tx = cx + math.cos(ang) * r
        ty = cy + math.sin(ang) * r
        text = str(hour)
        fnt = font_hour_12 if hour == 12 else font_hour
        # subtle shadow
        draw.text((tx + SCALE, ty + SCALE), text, fill=(0, 0, 0), font=fnt, anchor="mm")
        draw.text((tx, ty), text, fill=WHITE, font=fnt, anchor="mm")

    # Top weekday arc labels (static decoration)
    font_sm = load_font(9 * SCALE)
    days = "MTWTFSS"
    arc_cy = cy - 52 * SCALE
    for i, ch in enumerate(days):
        ang = math.radians(-110 + i * 11)
        tx = cx + math.cos(ang) * 38 * SCALE
        ty = arc_cy + math.sin(ang) * 12 * SCALE
        draw.text((tx, ty), ch, fill=GRAY, font=font_sm, anchor="mm")

    # Orange accent ring at bottom (decorative gauge hint)
    draw.arc(
        (cx - 36 * SCALE, cy + 18 * SCALE, cx + 36 * SCALE, cy + 54 * SCALE),
        200,
        340,
        fill=ORANGE,
        width=4 * SCALE,
    )

    # Calm centre for digital readout + hub (no hands)
    draw.ellipse(
        (cx - 42 * SCALE, cy - 42 * SCALE, cx + 42 * SCALE, cy + 42 * SCALE),
        fill=(14, 16, 20),
        outline=GRID,
        width=1,
    )

    masked = Image.new("RGB", (w, w), (0, 0, 0))
    mask = Image.new("L", (w, w), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, w - 1, w - 1), fill=255)
    masked.paste(img, mask=mask)
    return downscale(masked)


def skeleton_hand(
    width: int,
    height: int,
    *,
    tip_half: int,
    mid_half: int,
    base_half: int,
    cutout: bool = True,
) -> Image.Image:
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = width // 2
    bot = height - 1

    body = [
        (cx, 0),
        (cx + tip_half, 8),
        (cx + mid_half, bot - 18),
        (cx + base_half, bot),
        (cx - base_half, bot),
        (cx - mid_half, bot - 18),
        (cx - tip_half, 8),
    ]
    draw.polygon(body, fill=WHITE, outline=(30, 30, 34))

    if cutout:
        draw.rectangle((cx - 2, 12, cx + 2, bot - 22), fill=BLACK)
        draw.polygon(
            [(cx - 1, 16), (cx + 1, 16), (cx + mid_half - 2, bot - 24), (cx - mid_half + 2, bot - 24)],
            fill=BLACK,
        )

    return img


def draw_second_hand() -> Image.Image:
    w, h = 8, 106
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = w // 2
    draw.line((cx, 0, cx, h - 12), fill=ORANGE, width=2)
    draw.rectangle((cx - 2, h - 10, cx + 2, h - 6), fill=ORANGE_DIM)
    draw.ellipse((cx - 2, h - 6, cx + 2, h - 2), fill=ORANGE)
    return img


def draw_hub() -> Image.Image:
    size = 24
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    c = size // 2
    draw.ellipse((2, 2, size - 2, size - 2), fill=(18, 20, 24), outline=WHITE, width=2)
    draw.ellipse((c - 4, c - 4, c + 4, c + 4), outline=ORANGE, width=1)
    draw.ellipse((c - 1, c - 1, c + 1, c + 1), fill=WHITE)
    return img


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "source").mkdir(exist_ok=True)

    make_dial().save(OUT / "dial.png", optimize=True)
    skeleton_hand(16, 58, tip_half=3, mid_half=4, base_half=6).save(OUT / "hour.png")
    skeleton_hand(12, 88, tip_half=2, mid_half=3, base_half=5, cutout=True).save(OUT / "minute.png")
    draw_second_hand().save(OUT / "second.png")
    draw_hub().save(OUT / "hub.png")

    manifest = {
        "id": "wayfinder",
        "name": "Wayfinder",
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
        "behaviour": {
            "show_second_hand": True,
            "digital_readout": True,
        },
    }
    (OUT / "face.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Generated wayfinder face in {OUT}")


if __name__ == "__main__":
    main()

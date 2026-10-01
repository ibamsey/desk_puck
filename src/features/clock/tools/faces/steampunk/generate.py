#!/usr/bin/env python3
"""Generate steampunk watch face PNGs + face.json (procedural placeholder art)."""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))
from _paths import face_dir  # noqa: E402

OUT = face_dir("steampunk")
CANVAS = 240
CX, CY = 120, 120


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def rgb565_like(r: int, g: int, b: int) -> tuple[int, int, int]:
    return (r, g, b)


# Steampunk palette
BG_DARK = (28, 22, 18)
BG_MID = (52, 38, 28)
BRASS_DARK = (98, 72, 38)
BRASS_MID = (168, 128, 58)
BRASS_HI = (220, 188, 110)
COPPER = (184, 92, 48)
RUST = (120, 48, 32)
IVORY = (230, 210, 175)
STEEL = (140, 148, 158)
SECOND = (210, 60, 45)


def draw_gear(
    draw: ImageDraw.ImageDraw,
    cx: float,
    cy: float,
    outer_r: float,
    inner_r: float,
    teeth: int,
    fill: tuple[int, int, int],
    outline: tuple[int, int, int] | None = None,
) -> None:
    points: list[tuple[float, float]] = []
    for i in range(teeth * 2):
        ang = (i / (teeth * 2)) * 2 * math.pi - math.pi / 2
        r = outer_r if i % 2 == 0 else inner_r
        points.append((cx + math.cos(ang) * r, cy + math.sin(ang) * r))
    draw.polygon(points, fill=fill, outline=outline)


def make_dial() -> Image.Image:
    img = Image.new("RGB", (CANVAS, CANVAS), BG_DARK)
    px = img.load()
    for y in range(CANVAS):
        for x in range(CANVAS):
            dx, dy = x - CX, y - CY
            dist = math.hypot(dx, dy)
            if dist > 118:
                px[x, y] = (12, 10, 8)
                continue
            t = dist / 118
            r = int(lerp(BG_MID[0], BG_DARK[0], t * 0.85))
            g = int(lerp(BG_MID[1], BG_DARK[1], t * 0.85))
            b = int(lerp(BG_MID[2], BG_DARK[2], t * 0.85))
            # subtle radial "grain"
            r = min(255, max(0, r + int((math.sin(dist * 0.4) + math.cos((x + y) * 0.08)) * 4)))
            px[x, y] = (r, g, b)

    draw = ImageDraw.Draw(img)

    # Outer brass bezel ring
    draw.ellipse((8, 8, 231, 231), outline=BRASS_DARK, width=6)
    draw.ellipse((14, 14, 225, 225), outline=BRASS_MID, width=2)

    # Rivets
    for deg in range(0, 360, 30):
        rad = math.radians(deg)
        rx = CX + math.cos(rad) * 108
        ry = CY + math.sin(rad) * 108
        draw.ellipse((rx - 4, ry - 4, rx + 4, ry + 4), fill=BRASS_HI, outline=BRASS_DARK)
        draw.ellipse((rx - 1.5, ry - 1.5, rx + 1.5, ry + 1.5), fill=BRASS_DARK)

    # Decorative gears (static)
    draw_gear(draw, 58, 58, 22, 16, 8, BRASS_DARK, BRASS_MID)
    draw_gear(draw, 182, 58, 18, 13, 6, COPPER, BRASS_DARK)
    draw_gear(draw, 58, 182, 16, 11, 6, STEEL, (90, 95, 100))
    draw_gear(draw, 178, 178, 20, 14, 7, RUST, BRASS_DARK)

    # Hour ticks — Roman-inspired heavy ticks at 12/3/6/9, lighter elsewhere
    for i in range(60):
        ang = math.radians(i * 6 - 90)
        major = i % 5 == 0
        major12 = i % 15 == 0
        inner = 88 if major12 else (92 if major else 96)
        outer = 104 if major12 else (100 if major else 98)
        x0 = CX + math.cos(ang) * inner
        y0 = CY + math.sin(ang) * inner
        x1 = CX + math.cos(ang) * outer
        y1 = CY + math.sin(ang) * outer
        w = 4 if major12 else (3 if major else 1)
        col = BRASS_HI if major12 else (BRASS_MID if major else BRASS_DARK)
        draw.line((x0, y0, x1, y1), fill=col, width=w)

    # Inner dial ring
    draw.ellipse((36, 36, 203, 203), outline=BRASS_DARK, width=2)
    draw.ellipse((42, 42, 197, 197), outline=(70, 52, 36), width=1)

    # Sub-dial arcs (pressure gauge hint at 10 and 2 o'clock)
    for cx_off, cy_off in ((CX - 42, CY - 28), (CX + 42, CY - 28)):
        draw.arc((cx_off - 18, cy_off - 18, cx_off + 18, cy_off + 18), 200, 340, fill=COPPER, width=2)

    # Calm centre for hub
    draw.ellipse((CX - 14, CY - 14, CX + 14, CY + 14), fill=(38, 28, 22))

    # Mask to circle
    mask = Image.new("L", (CANVAS, CANVAS), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, CANVAS - 1, CANVAS - 1), fill=255)
    bg = Image.new("RGB", (CANVAS, CANVAS), (0, 0, 0))
    bg.paste(img, mask=mask)
    return bg


def draw_brass_hand(
    width: int,
    height: int,
    *,
    tip_half: int,
    base_half: int,
    spade: bool = False,
) -> Image.Image:
    """Hand points up; pivot at bottom-centre row."""
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = width // 2
    bot = height - 1

    # Body polygon (tapered)
    top_y = 0
    if spade and tip_half >= 2:
        top_y = 4
        draw.polygon(
            [
                (cx, 0),
                (cx - tip_half - 2, top_y + 6),
                (cx + tip_half + 2, top_y + 6),
            ],
            fill=BRASS_HI,
            outline=BRASS_DARK,
        )

    for y in range(top_y, bot):
        t = y / max(bot - top_y, 1)
        half = max(tip_half, int(round(tip_half + (base_half - tip_half) * t)))
        for x in range(width):
            if abs(x - cx) <= half:
                edge = abs(x - cx) == half or y == top_y
                px_col = BRASS_DARK if edge else (BRASS_HI if (x + y) % 3 == 0 else BRASS_MID)
                img.putpixel((x, y), (*px_col, 255))

    # Centre pin stripe
    draw.line((cx, top_y + 2, cx, bot - 2), fill=IVORY, width=1)
    return img


def draw_second_hand() -> Image.Image:
    w, h = 6, 102
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = w // 2
    draw.line((cx, 0, cx, h - 8), fill=SECOND, width=2)
    draw.ellipse((cx - 3, h - 10, cx + 3, h - 4), fill=COPPER, outline=BRASS_DARK)
    return img


def draw_hub() -> Image.Image:
    size = 28
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    c = size // 2
    draw_gear(draw, c, c, 12, 8, 6, BRASS_MID, BRASS_DARK)
    draw.ellipse((c - 5, c - 5, c + 5, c + 5), fill=BRASS_DARK, outline=BRASS_HI)
    draw.ellipse((c - 2, c - 2, c + 2, c + 2), fill=(40, 32, 24))
    # slot screw
    draw.line((c - 4, c, c + 4, c), fill=BRASS_HI, width=2)
    return img


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "source").mkdir(exist_ok=True)

    make_dial().save(OUT / "dial.png")
    draw_brass_hand(14, 58, tip_half=2, base_half=6, spade=True).save(OUT / "hour.png")
    draw_brass_hand(10, 88, tip_half=1, base_half=4, spade=False).save(OUT / "minute.png")
    draw_second_hand().save(OUT / "second.png")
    draw_hub().save(OUT / "hub.png")

    manifest = {
        "id": "steampunk",
        "name": "Steampunk",
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
    print(f"Generated steampunk face in {OUT}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Classic chronograph: 9h running seconds, 12h chrono minutes, 6h chrono seconds."""

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

OUT = face_dir("classic-chrono")
CANVAS = 240
CX, CY = 120, 120
SUB_WALL = (52, 120)
SUB_MIN = (120, 52)
SUB_CHRONO_SEC = (120, 168)

WHITE = (245, 245, 240)
BLACK = (18, 18, 20)
SILVER = (190, 192, 198)
RED = (210, 45, 38)
BLUE = (28, 48, 120)


def make_dial() -> Image.Image:
    img = Image.new("RGB", (CANVAS, CANVAS), BLACK)
    draw = ImageDraw.Draw(img)

    for y in range(CANVAS):
        for x in range(CANVAS):
            if (x - CX) ** 2 + (y - CY) ** 2 > 118**2:
                img.putpixel((x, y), (8, 8, 10))

    draw.ellipse((6, 6, 233, 233), outline=SILVER, width=4)
    draw.ellipse((12, 12, 227, 227), outline=(120, 122, 128), width=1)

    for i in range(60):
        ang = math.radians(i * 6 - 90)
        major = i % 5 == 0
        inner = 92 if major else 96
        outer = 108 if major else 102
        w = 3 if major else 1
        col = WHITE if major else (100, 100, 105)
        x0 = CX + math.cos(ang) * inner
        y0 = CY + math.sin(ang) * inner
        x1 = CX + math.cos(ang) * outer
        y1 = CY + math.sin(ang) * outer
        draw.line((x0, y0, x1, y1), fill=col, width=w)

    def subdial_ring(cx: int, cy: int, r: int) -> None:
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), outline=SILVER, width=2)
        draw.ellipse((cx - r + 4, cy - r + 4, cx + r - 4, cy + r - 4), outline=(60, 60, 65), width=1)
        for j in range(0, 360, 30):
            a = math.radians(j - 90)
            x0 = cx + math.cos(a) * (r - 8)
            y0 = cy + math.sin(a) * (r - 8)
            x1 = cx + math.cos(a) * (r - 2)
            y1 = cy + math.sin(a) * (r - 2)
            draw.line((x0, y0, x1, y1), fill=WHITE, width=1)

    subdial_ring(*SUB_WALL, 22)
    subdial_ring(*SUB_MIN, 26)
    subdial_ring(*SUB_CHRONO_SEC, 28)

    draw.arc(
        (SUB_CHRONO_SEC[0] - 30, SUB_CHRONO_SEC[1] - 30, SUB_CHRONO_SEC[0] + 30, SUB_CHRONO_SEC[1] + 30),
        200,
        340,
        fill=BLUE,
        width=3,
    )

    draw.ellipse((CX - 12, CY - 12, CX + 12, CY + 12), fill=(28, 28, 32))
    return img


def draw_hand(width: int, height: int, tip: int, base: int, color: tuple[int, int, int, int]) -> Image.Image:
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    px = img.load()
    cx = width // 2
    for y in range(height):
        t = y / max(height - 1, 1)
        half = max(tip, int(round(tip + (base - tip) * t)))
        for x in range(width):
            if abs(x - cx) <= half:
                px[x, y] = color
    return img


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    make_dial().save(OUT / "dial.png")

    draw_hand(12, 56, 2, 5, (*WHITE, 255)).save(OUT / "hour.png")
    draw_hand(8, 84, 1, 4, (*WHITE, 255)).save(OUT / "minute.png")
    draw_hand(6, 40, 0, 2, (*SILVER, 255)).save(OUT / "sub_wall_sec.png")
    draw_hand(6, 44, 0, 2, (*WHITE, 255)).save(OUT / "sub_chrono_min.png")
    draw_hand(6, 48, 0, 3, (*RED, 255)).save(OUT / "sub_chrono_sec.png")

    hub = Image.new("RGBA", (20, 20), (0, 0, 0, 0))
    d = ImageDraw.Draw(hub)
    d.ellipse((2, 2, 17, 17), fill=(40, 40, 45, 255), outline=(220, 220, 225, 255))
    d.ellipse((8, 8, 11, 11), fill=(180, 180, 185, 255))
    hub.save(OUT / "hub.png")

    manifest = {
        "id": "classic-chrono",
        "name": "Classic Chrono",
        "version": 1,
        "pivot": {"x": 120, "y": 120},
        "behaviour": {"chronograph": True},
        "hands": {
            "hour": {"file": "hour.png", "length_px": 50, "offset_deg": 0},
            "minute": {"file": "minute.png", "length_px": 76, "offset_deg": 0},
        },
        "subdials": [
            {
                "role": "wall_second",
                "file": "sub_wall_sec.png",
                "subdial": {"x": SUB_WALL[0], "y": SUB_WALL[1]},
            },
            {
                "role": "chrono_minute",
                "file": "sub_chrono_min.png",
                "subdial": {"x": SUB_MIN[0], "y": SUB_MIN[1]},
            },
            {
                "role": "chrono_second",
                "file": "sub_chrono_sec.png",
                "subdial": {"x": SUB_CHRONO_SEC[0], "y": SUB_CHRONO_SEC[1]},
            },
        ],
        "hub": {"file": "hub.png"},
    }
    (OUT / "face.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {OUT}")


if __name__ == "__main__":
    main()

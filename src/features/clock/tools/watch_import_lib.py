"""WatchMaker .watch → Desk Puck asset face (shared library)."""

from __future__ import annotations

import base64
import io
import json
import math
import re
import shutil
import zipfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

DESK = 240
HAND_MAX_W, HAND_MAX_H = 48, 110
FACE_PIVOT_DESK = (120, 120)

# WatchMaker rotation expressions → hand role
HAND_ROTATION_TOKENS: dict[str, tuple[str, ...]] = {
    "hour": ("{drh}", "{bh}", "{h}", "rotate_hour"),
    "minute": ("{drm}", "{bm}", "{m}", "rotate_min"),
    "second": ("{drss}", "{drs}", "{ds}", "{s}", "rotate_sec"),
}

# Off-centre rotating layers → firmware subdial roles (match chrono before {drs} / {drss} overlap).
SUBDIAL_ROTATION_TOKENS: dict[str, tuple[str, ...]] = {
    "chrono_minute": ("{swm}", "swm"),
    "chrono_second": ("{drss}", "{sws}", "sws"),
    "wall_second": ("{drs}", "{ds}"),
}

# WatchMaker x/y offset from face centre (512 canvas) below this → main hour/minute/second hand.
MAIN_HAND_PIVOT_WM_TOL = 8

# Minimum crop radius (WatchMaker canvas px) when auto-sizing hand patches.
MIN_HAND_PATCH_HALF_WM = 36
HAND_PATCH_MARGIN_WM = 8

_PXML_PATH_RE = re.compile(r"(\.[Ii]mg[\w.]+\.(?:ppng|png))")
_PXML_FUZZY_IMG_RE = re.compile(r"(\.img\d+).{0,6}(ppng|png)", re.I)


@dataclass
class HandDiscovery:
    path: str
    layer: ET.Element


@dataclass
class SubdialDiscovery:
    role: str
    path: str
    layer: ET.Element


@dataclass
class ImportResult:
    slug: str
    out_dir: Path
    notes: list[str] = field(default_factory=list)


def slugify(text: str) -> str:
    s = text.lower().strip()
    s = re.sub(r"\.watch$", "", s, flags=re.I)
    s = re.sub(r"[^a-z0-9]+", "-", s)
    s = re.sub(r"-+", "-", s).strip("-")
    return s or "imported-watch"


def wm_canvas_size(zf: zipfile.ZipFile) -> int:
    for name in ("preview.jpg", "preview_dim.jpg"):
        if name in zf.namelist():
            im = Image.open(io.BytesIO(zf.read(name)))
            return max(im.size)
    return 512


def hex_rgb(color: str) -> tuple[int, int, int, int]:
    c = re.sub(r"[^0-9a-fA-F]", "", (color or "000000"))
    if len(c) == 6:
        return int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16), 255
    return 0, 0, 0, 255


def is_encrypted_watchmaker_image(data: bytes) -> bool:
    """WatchMaker protected exports use .ppng blobs, not PNG (magic da 07 06)."""
    return len(data) >= 3 and data[0:3] == b"\xda\x07\x06"


def assert_assets_readable(zf: zipfile.ZipFile, watch_name: str, paths: set[str]) -> None:
    encrypted: list[str] = []
    for path in paths:
        key = f"images/{path.lstrip('/')}"
        if key not in zf.namelist():
            continue
        data = zf.read(key)
        if is_encrypted_watchmaker_image(data):
            encrypted.append(path)
    if encrypted:
        raise SystemExit(
            f"{watch_name}: layer images are WatchMaker-protected (.ppng encrypted). "
            "Re-export the face from WatchMaker with protection turned off, or use an "
            "unprotected .watch from the designer. Preview JPG is not enough for separate hands."
        )


def load_image(zf: zipfile.ZipFile, path: str) -> Image.Image:
    rel = path.lstrip("/")
    names = zf.namelist()
    candidates = [rel]
    if rel.endswith(".png"):
        candidates.append(rel[:-4] + ".ppng")
    elif rel.endswith(".ppng"):
        candidates.append(rel[:-5] + ".png")
    for rel_path in candidates:
        key = f"images/{rel_path}"
        if key in names:
            data = zf.read(key)
            if is_encrypted_watchmaker_image(data):
                raise SystemExit(
                    f"WatchMaker-protected image {rel_path} — re-export the .watch with "
                    "protection disabled (WatchMaker: share/export without layer lock)."
                )
            return Image.open(io.BytesIO(data)).convert("RGBA")
    raise KeyError(path)


def _layer_opacity(layer: ET.Element) -> int:
    raw = (layer.attrib.get("opacity") or "100").strip()
    try:
        return int(float(raw))
    except ValueError:
        return 0


def apply_watchmaker_layer_tint(img: Image.Image, layer: ET.Element) -> Image.Image:
    """WatchMaker hand PNGs are usually white masks; layer ``color`` tints them (e.g. gold ebdb74)."""
    src = img.convert("RGBA")
    color = (layer.attrib.get("color") or "").strip()
    if not color:
        return src
    tr, tg, tb, _ = hex_rgb(color)
    opacity = _layer_opacity(layer) / 100.0
    out = Image.new("RGBA", src.size)
    sp = src.load()
    opx = out.load()
    for y in range(src.height):
        for x in range(src.width):
            pr, pg, pb, pa = sp[x, y]
            if pa < 8:
                opx[x, y] = (0, 0, 0, 0)
                continue
            lum = (pr + pg + pb) / (3.0 * 255.0)
            na = int(round(pa * opacity))
            opx[x, y] = (min(255, int(tr * lum)), min(255, int(tg * lum)), min(255, int(tb * lum)), na)
    return out


def _hand_layer_rank(layer: ET.Element) -> int:
    """Prefer plain colour hands over WatchMaker shader/mask duplicate layers."""
    rank = _layer_opacity(layer)
    if layer.attrib.get("shader"):
        rank -= 1000
    return rank


def decode_watch_pxml(zf: zipfile.ZipFile) -> str:
    """WatchMaker Pro stores layers in base64 watch.pxml (0x03 byte separators)."""
    raw = zf.read("watch.pxml").decode("ascii").strip()
    payload = base64.b64decode(raw)
    cleaned = bytes(b for b in payload if b != 0x03)
    return cleaned.decode("latin-1")


def _attr_from_chunk(chunk: str, *names: str) -> str:
    for name in names:
        m = re.search(rf'\b{re.escape(name)}="([^"]*)"', chunk)
        if m:
            return m.group(1)
    return ""


def _image_names_in_zip(zf: zipfile.ZipFile) -> set[str]:
    return {Path(n).name for n in zf.namelist() if n.startswith("images/")}


def _path_from_layer_chunk(chunk: str, image_names: set[str] | None = None) -> str:
    path = _attr_from_chunk(chunk, "path")
    if path:
        return path
    m = re.search(r"patj\s*\r?\n\s*\"([^\"]*)\"", chunk)
    if m:
        return m.group(1)
    m = _PXML_PATH_RE.search(chunk)
    if m:
        return m.group(1)
    if image_names:
        ids = re.findall(r"\.img(\d+)", chunk, re.I)
        best = ""
        best_id = -1
        for num in ids:
            for ext in (".ppng", ".png"):
                exact = f".img{num}{ext}"
                if exact in image_names:
                    img_id = int(num)
                    if img_id > best_id:
                        best = exact
                        best_id = img_id
            prefix = f".img{num}"
            for name in image_names:
                m = re.match(r"\.img(\d+)\.(?:ppng|png)$", name, re.I)
                if not m or not name.startswith(prefix):
                    continue
                img_id = int(m.group(1))
                if img_id > best_id:
                    best = name
                    best_id = img_id
        if best:
            return best
    m = _PXML_FUZZY_IMG_RE.search(chunk)
    if m:
        return f"{m.group(1)}.{m.group(2).lower()}"
    return ""


def _layer_x_y(chunk: str) -> tuple[str, str]:
    x = _attr_from_chunk(chunk, "x", "K")
    y = _attr_from_chunk(chunk, "y")
    if not x:
        m = re.search(r'\bz\s*\r?\n\s*"(-?[0-9.]+)"', chunk)
        if m:
            x = m.group(1)
    if not y and x:
        m = re.search(r'\by="(-?[0-9.]+)"', chunk)
        if m:
            y = m.group(1)
    return x or "0", y or "0"


def watch_root_from_zip(zf: zipfile.ZipFile) -> ET.Element:
    """Parse watch.xml, or rebuild layers from watch.pxml when XML is metadata-only."""
    xml_bytes = zf.read("watch.xml")
    try:
        root = ET.fromstring(xml_bytes)
    except ET.ParseError:
        root = ET.Element("Watch")
    if root.findall("Layer"):
        return root
    if "watch.pxml" not in zf.namelist():
        return root

    text = decode_watch_pxml(zf)
    head = re.search(r"<Watch\s+([^/>]+)", text)
    attribs: dict[str, str] = {"name": root.attrib.get("name", "")}
    if head:
        for m in re.finditer(r'(\w+)="([^"]*)"', head.group(1)):
            val = re.sub(r"[\x00-\x1f]", "", m.group(2))
            attribs[m.group(1)] = val
    out = ET.Element("Watch", {k: v for k, v in attribs.items() if v})
    image_names = _image_names_in_zip(zf)

    for chunk in text.split("<Layer"):
        if 'type="image"' not in chunk[:160] and 'type="text"' not in chunk[:160]:
            if 'type="shape"' not in chunk[:160] and 'type="markers"' not in chunk[:160]:
                continue
        layer_type = _attr_from_chunk(chunk, "type") or "image"
        attrs: dict[str, str] = {"type": layer_type}
        if layer_type == "image":
            path = _path_from_layer_chunk(chunk, image_names)
            if path:
                attrs["path"] = path
            rot = _attr_from_chunk(chunk, "rotation")
            if rot:
                attrs["rotation"] = re.sub(r"[\x00-\x1f]", "", rot)
            x, y = _layer_x_y(chunk)
            attrs["x"], attrs["y"] = x, y
            for key in ("opacity", "display", "width", "height"):
                val = _attr_from_chunk(chunk, key)
                if val:
                    attrs[key] = re.sub(r"[\x00-\x1f]", "", val)
        elif layer_type == "text":
            for key in ("text", "x", "y", "text_size", "color", "font", "display"):
                val = _attr_from_chunk(chunk, key)
                if not val and key in ("x", "y"):
                    val = _layer_x_y(chunk)[0 if key == "x" else 1]
                if val:
                    attrs[key] = val
        else:
            for key in ("x", "y", "rotation", "opacity", "display", "shape", "width", "height", "color"):
                val = _attr_from_chunk(chunk, key)
                if val:
                    attrs[key] = val
        ET.SubElement(out, "Layer", attrs)
    return out


def rotation_matches(role: str, rotation: str) -> bool:
    rot = rotation.lower()
    for token in HAND_ROTATION_TOKENS[role]:
        t = token.lower()
        if t not in rot:
            continue
        # Avoid matching {drs} inside {drss} for second-hand discovery.
        if role == "second" and t in ("{drs}", "{ds}", "{s}") and "{drss}" in rot:
            continue
        return True
    return False


def parse_wm_number(raw: str, default: float = 0.0) -> float:
    """Parse a WatchMaker numeric field (coords, size, rotation)."""
    if raw is None:
        return default
    parsed = _parse_wm_coord(str(raw))
    return default if parsed is None else parsed


def _parse_wm_coord(raw: str) -> float | None:
    """Parse WatchMaker layer x/y; tolerate junk after null bytes and *var_scale offsets."""
    s = re.sub(r"[\x00-\x1f].*", "", (raw or "0").strip())
    if not s:
        return 0.0
    m = re.match(r"^-?\d+(?:\.\d+)?", s)
    if m:
        return float(m.group(0))
    m = re.match(r"^(-?\d+(?:\.\d+)?)\*var_scale", s.replace(" ", ""), re.I)
    if m:
        return float(m.group(1))
    return None


def layer_pivot_wm(layer: ET.Element) -> tuple[float, float]:
    x = layer.attrib.get("x", layer.attrib.get("K", "0"))
    y = layer.attrib.get("y", "0")
    px = _parse_wm_coord(x)
    py = _parse_wm_coord(y)
    if px is None or py is None:
        raise ValueError(f"unparseable pivot x={x!r} y={y!r}")
    return px, py


def is_main_hand_pivot(layer: ET.Element) -> bool:
    try:
        x, y = layer_pivot_wm(layer)
    except ValueError:
        return False
    return abs(x) <= MAIN_HAND_PIVOT_WM_TOL and abs(y) <= MAIN_HAND_PIVOT_WM_TOL


def subdial_role_from_rotation(rotation: str) -> str | None:
    rot = rotation.lower()
    for role, tokens in SUBDIAL_ROTATION_TOKENS.items():
        for token in tokens:
            if token.lower() in rot:
                return role
    return None


def find_hand_layer(root: ET.Element, png_path: str, role: str) -> ET.Element | None:
    best: ET.Element | None = None
    for layer in root.findall("Layer"):
        if layer.attrib.get("type") != "image" or layer.attrib.get("path") != png_path:
            continue
        if not rotation_matches(role, layer.attrib.get("rotation", "")):
            continue
        if layer.attrib.get("display") == "d":
            continue
        rank = _hand_layer_rank(layer)
        if best is None or rank >= _hand_layer_rank(best):
            best = layer
    return best


def _main_hand_layers_for_role(root: ET.Element, role: str) -> list[ET.Element]:
    """All centre-pivot layers for a hand role (gold + shader masks), bottom → top."""
    layers: list[ET.Element] = []
    for layer in root.findall("Layer"):
        if layer.attrib.get("type") != "image":
            continue
        if not is_main_hand_pivot(layer):
            continue
        if not rotation_matches(role, layer.attrib.get("rotation", "")):
            continue
        if layer.attrib.get("display") == "d":
            continue
        if not layer.attrib.get("path"):
            continue
        layers.append(layer)
    layers.sort(key=lambda layer: (1 if layer.attrib.get("shader") else 0, -_hand_layer_rank(layer)))
    return layers


def discover_hands(root: ET.Element) -> dict[str, HandDiscovery]:
    """Primary layer per role (for metadata); sprite import composites all matching layers."""
    found: dict[str, HandDiscovery] = {}
    for role in ("hour", "minute", "second"):
        stack = _main_hand_layers_for_role(root, role)
        if not stack:
            continue
        primary = stack[-1]
        found[role] = HandDiscovery(path=primary.attrib.get("path", ""), layer=primary)
    return found


def discover_subdials(root: ET.Element) -> list[SubdialDiscovery]:
    """Off-centre image layers with subdial rotation tokens → role + desk pivot from x/y."""
    best_by_role: dict[str, SubdialDiscovery] = {}
    skipped: list[str] = []

    for layer in root.findall("Layer"):
        if layer.attrib.get("type") != "image":
            continue
        if is_main_hand_pivot(layer):
            continue
        if layer.attrib.get("display") == "d":
            continue
        rot = layer.attrib.get("rotation", "")
        role = subdial_role_from_rotation(rot)
        if role is None:
            if "{" in rot or "rotate" in rot.lower():
                x, y = layer_pivot_wm(layer)
                skipped.append(f"{rot[:40]} @ wm({x:g},{y:g})")
            continue
        path = layer.attrib.get("path", "")
        if not path:
            continue
        rank = _hand_layer_rank(layer)
        prev = best_by_role.get(role)
        if prev is not None and rank < _hand_layer_rank(prev.layer):
            continue
        best_by_role[role] = SubdialDiscovery(role=role, path=path, layer=layer)

    order = ("wall_second", "chrono_minute", "chrono_second")
    found = [best_by_role[r] for r in order if r in best_by_role]
    return found, skipped


def dial_anchor_from_layer(layer: ET.Element, wm_size: int) -> tuple[int, int]:
    """WatchMaker cc: pivot at canvas centre + (x, y)."""
    x = parse_wm_number(layer.attrib.get("x", "0"))
    y = parse_wm_number(layer.attrib.get("y", "0"))
    ax = (wm_size / 2.0) + x
    ay = (wm_size / 2.0) + y
    return int(round(ax * DESK / wm_size)), int(round(ay * DESK / wm_size))


def anchor_if_not_face_center(anchor: tuple[int, int]) -> dict[str, int] | None:
    if anchor == FACE_PIVOT_DESK:
        return None
    return {"x": anchor[0], "y": anchor[1]}


def wm_coord_to_desk(x: float, y: float, wm_size: int) -> tuple[int, int]:
    ax = (wm_size / 2.0) + x
    ay = (wm_size / 2.0) + y
    return int(round(ax * DESK / wm_size)), int(round(ay * DESK / wm_size))


def rgb565_from_wm_color(color: str) -> str:
    r, g, b, _ = hex_rgb(color)
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return f"0x{v:04X}"


def wm_text_size_to_desk(text_size: float, wm_size: int) -> int:
    """Map WatchMaker pixel text_size to LovyanGFX setTextSize (roughly 6px per unit wide)."""
    px = text_size * DESK / wm_size
    return max(1, min(4, int(round(px / 6.0))))


# WatchMaker live text tokens we map to firmware digital readout (not baked into dial).
WM_DATE_TOKENS: dict[str, str] = {
    "{dd}": "day",
    "{d}": "day_unpadded",
}


def first_gif_frame_path(path_attr: str) -> str:
    """WatchMaker image_gif layers list many .jpg frames; use the first for a static dial."""
    m = re.search(r"(\.img\d+\.jpg)", path_attr, re.I)
    return m.group(1) if m else ""


def skip_import_overlay_layer(layer: ET.Element) -> bool:
    """Runtime overlays (weather) and dropped widgets (battery) must not bake into dial.png."""
    if layer.attrib.get("type") != "image_cond":
        return False
    path = (layer.attrib.get("path") or "").lower()
    return "battery_set" in path or "weather_set" in path


def discover_weather_icon(root: ET.Element, wm_size: int) -> dict | None:
    for layer in root.findall("Layer"):
        if layer.attrib.get("type") != "image_cond":
            continue
        path = layer.attrib.get("path", "")
        if "weather_set" not in path.lower():
            continue
        grid = (layer.attrib.get("cond_grid") or "3x3").lower().split("x")
        cols = int(grid[0]) if grid else 3
        rows = int(grid[1]) if len(grid) > 1 else 3
        sx, sy = dial_anchor_from_layer(layer, wm_size)
        w_wm = parse_wm_number(layer.attrib.get("width", "116"), 116)
        display = max(24, int(round(w_wm * DESK / wm_size)))
        return {
            "file": "weather_icons.png",
            "grid_cols": cols,
            "grid_rows": rows,
            "x": sx,
            "y": sy,
            "display_size": display,
        }
    return None


def discover_watchmaker_digital_readout(root: ET.Element, wm_size: int) -> dict | None:
    """Map WatchMaker live text layers ({dh}, {ddw}, …) → behaviour.digital_readout."""
    time_layer: ET.Element | None = None
    date_layer: ET.Element | None = None
    for layer in root.findall("Layer"):
        if layer.attrib.get("type") != "text":
            continue
        if layer.attrib.get("display", "bd") == "d":
            continue
        token = layer.attrib.get("text", "").strip().lower()
        if not token:
            continue
        if any(t in token for t in ("{dh}", "{drh}", "{th}", "{tm}")):
            time_layer = layer
        if any(t in token for t in ("{ddw}", "{dnnn}", "{dd}", "{dw}")):
            date_layer = layer

    if time_layer is None and date_layer is None:
        return discover_watchmaker_date_readout(root, wm_size)

    cfg: dict = {
        "show_time": time_layer is not None,
        "show_date": date_layer is not None,
        "time_x": 120,
        "time_y": 108,
        "date_x": 120,
        "date_y": 132,
        "time_color": "0xFFFF",
        "date_color": "0xFFFF",
        "bg_color": "0x0000",
        "time_text_size": 2,
        "date_text_size": 1,
    }
    if time_layer is not None:
        tx, ty = dial_anchor_from_layer(time_layer, wm_size)
        cfg["time_x"] = tx
        cfg["time_y"] = ty
        cfg["time_color"] = rgb565_from_wm_color(time_layer.attrib.get("color", "ffffff"))
        ts = parse_wm_number(time_layer.attrib.get("text_size", "24"), 24)
        cfg["time_text_size"] = wm_text_size_to_desk(ts, wm_size)
        if "{dh}" in time_layer.attrib.get("text", "").lower():
            cfg["time_format"] = "12h_ampm"
    if date_layer is not None:
        dx, dy = dial_anchor_from_layer(date_layer, wm_size)
        cfg["date_x"] = dx
        cfg["date_y"] = dy
        cfg["date_color"] = rgb565_from_wm_color(date_layer.attrib.get("color", "ffffff"))
        ds = parse_wm_number(date_layer.attrib.get("text_size", "24"), 24)
        cfg["date_text_size"] = wm_text_size_to_desk(ds, wm_size)
        tok = date_layer.attrib.get("text", "").lower()
        if "{ddw}" in tok and "{dnnn}" in tok:
            cfg["date_format"] = "weekday_month_day"
        elif "{dd}" in tok:
            cfg["date_format"] = "day"
    return cfg


def discover_watchmaker_date_readout(root: ET.Element, wm_size: int) -> dict | None:
    """First bright-mode date token layer ({dd}, {d}) → digital_readout for import."""
    for layer in root.findall("Layer"):
        if layer.attrib.get("type") != "text":
            continue
        if layer.attrib.get("display", "bd") == "d":
            continue
        token = layer.attrib.get("text", "").strip().lower()
        date_format = WM_DATE_TOKENS.get(token)
        if not date_format:
            continue
        x = parse_wm_number(layer.attrib.get("x", "0"))
        y = parse_wm_number(layer.attrib.get("y", "0"))
        date_x, date_y = wm_coord_to_desk(x, y, wm_size)
        text_size = parse_wm_number(layer.attrib.get("text_size", "24"), 24)
        return {
            "show_time": False,
            "show_date": True,
            "date_format": date_format,
            "date_x": date_x,
            "date_y": date_y,
            "date_color": rgb565_from_wm_color(layer.attrib.get("color", "ffffff")),
            "date_text_size": wm_text_size_to_desk(text_size, wm_size),
            "time_x": 120,
            "time_y": 108,
            "time_color": "0xFFFF",
            "time_text_size": 1,
            "bg_color": "0x0000",
        }
    return None


class WatchCompositor:
    def __init__(
        self,
        wm_size: int,
        hand_paths: set[str],
        *,
        strip_full_frame_jpg: bool = False,
    ) -> None:
        self.wm_size = wm_size
        self.cx = wm_size // 2
        self.cy = wm_size // 2
        self.hand_paths = hand_paths
        self.strip_full_frame_jpg = strip_full_frame_jpg
        self.skipped_dial_layers: list[str] = []

    def is_dynamic(self, layer: ET.Element) -> bool:
        rot = layer.attrib.get("rotation", "")
        if "{" in rot:
            return True
        if layer.attrib.get("type") == "text" and "{" in layer.attrib.get("text", ""):
            return True
        if layer.attrib.get("path", "") in self.hand_paths:
            return True
        return False

    def skip_static_layer(self, layer: ET.Element, zf: zipfile.ZipFile) -> bool:
        """Drop static centre overlays (hand masks/hubs) that duplicate live hands."""
        if layer.attrib.get("type") != "image":
            return False
        rot = (layer.attrib.get("rotation") or "0").strip()
        if rot not in ("0", "0.0", ""):
            return False
        if not is_main_hand_pivot(layer):
            return False
        path = layer.attrib.get("path", "")
        if not path:
            return False
        try:
            img = load_image(zf, path)
        except KeyError:
            return False
        bbox = img.getbbox()
        if not bbox:
            return True
        area = (bbox[2] - bbox[0]) * (bbox[3] - bbox[1])
        # Small centred PNGs are usually hand masks, not dial art (e.g. D&G .img4619).
        if area < (self.wm_size * 0.22) ** 2:
            return True
        if self.strip_full_frame_jpg and path.lower().endswith(".jpg"):
            if max(img.size) >= int(self.wm_size * 0.9):
                self.skipped_dial_layers.append(path)
                return True
        return False

    def blit_image(
        self,
        canvas: Image.Image,
        img: Image.Image,
        x: float,
        y: float,
        rotation: float,
        width: float | None,
        height: float | None,
    ) -> None:
        if width and height:
            tw, th = int(round(parse_wm_number(str(width)))), int(round(parse_wm_number(str(height))))
            if (tw, th) != img.size:
                img = img.resize((tw, th), Image.Resampling.LANCZOS)
        if rotation:
            img = img.rotate(-rotation, resample=Image.Resampling.BICUBIC, expand=True)
        px = int(round(self.cx + x - img.width / 2))
        py = int(round(self.cy + y - img.height / 2))
        canvas.alpha_composite(img, (px, py))

    def draw_shape(self, canvas: Image.Image, layer: ET.Element) -> None:
        shape = layer.attrib.get("shape", "Circle")
        w = int(round(parse_wm_number(layer.attrib.get("width", "100"), 100)))
        h = int(round(parse_wm_number(layer.attrib.get("height", "100"), 100)))
        x = parse_wm_number(layer.attrib.get("x", "0"))
        y = parse_wm_number(layer.attrib.get("y", "0"))
        color = hex_rgb(layer.attrib.get("color", "ffffff"))
        overlay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        if shape.lower() == "circle":
            draw.ellipse((0, 0, w - 1, h - 1), fill=color)
        else:
            draw.rectangle((0, 0, w - 1, h - 1), fill=color)
        self.blit_image(
            canvas, overlay, x, y, parse_wm_number(layer.attrib.get("rotation", "0")), w, h
        )

    def draw_markers(self, canvas: Image.Image, layer: ET.Element) -> None:
        radius = parse_wm_number(layer.attrib.get("radius", "0"))
        count = max(1, int(layer.attrib.get("m_count", 1)))
        mw = max(1, int(round(parse_wm_number(layer.attrib.get("m_width", "4"), 4))))
        mh = max(1, int(round(parse_wm_number(layer.attrib.get("m_height", "10"), 10))))
        base_rot = parse_wm_number(layer.attrib.get("rotation", "0"))
        ox = parse_wm_number(layer.attrib.get("x", "0"))
        oy = parse_wm_number(layer.attrib.get("y", "0"))
        color = hex_rgb(layer.attrib.get("color", "ffffff"))
        draw = ImageDraw.Draw(canvas)
        for i in range(count):
            ang = math.radians(base_rot + i * (360.0 / count) - 90)
            mx = self.cx + ox + math.cos(ang) * radius
            my = self.cy + oy + math.sin(ang) * radius
            draw.rectangle(
                (mx - mw / 2, my - mh / 2, mx + mw / 2 - 1, my + mh / 2 - 1),
                fill=color,
            )

    def draw_text_layer(self, canvas: Image.Image, layer: ET.Element, zf: zipfile.ZipFile) -> None:
        text = layer.attrib.get("text", "")
        if "{" in text:
            return
        size = int(round(parse_wm_number(layer.attrib.get("text_size", "16"), 16)))
        font_id = layer.attrib.get("font", "10")
        font_path = f"fonts/{font_id}.ttf"
        try:
            font = ImageFont.truetype(io.BytesIO(zf.read(font_path)), size)
        except KeyError:
            font = ImageFont.load_default()
        x = parse_wm_number(layer.attrib.get("x", "0"))
        y = parse_wm_number(layer.attrib.get("y", "0"))
        color = hex_rgb(layer.attrib.get("color", "ffffff"))[:3]
        overlay = Image.new("RGBA", (self.wm_size, self.wm_size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        draw.text((self.cx + x, self.cy + y), text, font=font, fill=color, anchor="mm")
        canvas.alpha_composite(overlay)

    def compose_dial(self, root: ET.Element, zf: zipfile.ZipFile) -> Image.Image:
        bg = hex_rgb(root.attrib.get("bg_color", "000000"))
        canvas = Image.new("RGBA", (self.wm_size, self.wm_size), bg)
        for layer in root.findall("Layer"):
            if layer.attrib.get("display", "bd") == "d":
                continue
            # Invisible tap targets (opacity 0) must not bake into dial.png (e.g. koboldpt chrono zones).
            if _layer_opacity(layer) <= 0:
                continue
            if skip_import_overlay_layer(layer):
                continue
            if self.is_dynamic(layer):
                continue
            if self.skip_static_layer(layer, zf):
                continue
            t = layer.attrib.get("type")
            if t == "image_gif":
                frame = first_gif_frame_path(layer.attrib.get("path", ""))
                if not frame:
                    continue
                try:
                    img = load_image(zf, frame)
                except KeyError:
                    continue
                self.blit_image(
                    canvas,
                    img,
                    parse_wm_number(layer.attrib.get("x", "0")),
                    parse_wm_number(layer.attrib.get("y", "0")),
                    parse_wm_number(layer.attrib.get("rotation", "0") or "0"),
                    layer.attrib.get("width"),
                    layer.attrib.get("height"),
                )
            elif t == "image":
                path = layer.attrib.get("path", "")
                try:
                    img = load_image(zf, path)
                except KeyError:
                    continue
                self.blit_image(
                    canvas,
                    img,
                    parse_wm_number(layer.attrib.get("x", "0")),
                    parse_wm_number(layer.attrib.get("y", "0")),
                    parse_wm_number(layer.attrib.get("rotation", "0") or "0"),
                    layer.attrib.get("width"),
                    layer.attrib.get("height"),
                )
            elif t == "shape":
                self.draw_shape(canvas, layer)
            elif t == "markers":
                self.draw_markers(canvas, layer)
            elif t == "text":
                self.draw_text_layer(canvas, layer, zf)
        return canvas


def _layer_image_for_blit(zf: zipfile.ZipFile, layer: ET.Element) -> Image.Image:
    path = layer.attrib.get("path", "")
    img = load_image(zf, path)
    w_attr = layer.attrib.get("width")
    h_attr = layer.attrib.get("height")
    if w_attr and h_attr:
        tw, th = int(round(parse_wm_number(str(w_attr)))), int(round(parse_wm_number(str(h_attr))))
        if (tw, th) != img.size:
            img = img.resize((tw, th), Image.Resampling.LANCZOS)
    return img


def _hand_content_radius_wm(
    canvas: Image.Image,
    pivot_wx: float,
    pivot_wy: float,
    wm_size: int,
) -> int:
    """Farthest opaque pixel from rotation pivot after layer is cc-blitted."""
    best = 0.0
    scan = min(wm_size // 2, 280)
    px_i = int(round(pivot_wx))
    py_i = int(round(pivot_wy))
    x0 = max(0, px_i - scan)
    y0 = max(0, py_i - scan)
    x1 = min(wm_size, px_i + scan)
    y1 = min(wm_size, py_i + scan)
    for yy in range(y0, y1):
        for xx in range(x0, x1):
            if canvas.getpixel((xx, yy))[3] < 16:
                continue
            best = max(best, math.hypot(xx - pivot_wx, yy - pivot_wy))
    radius = int(math.ceil(best)) + HAND_PATCH_MARGIN_WM
    return max(MIN_HAND_PATCH_HALF_WM, min(scan, radius))


def _tight_hand_crop(
    canvas: Image.Image,
    pivot_wx: float,
    pivot_wy: float,
    wm_size: int,
) -> tuple[Image.Image, float, float]:
    """Crop to opaque pixels near pivot (not a square window — keeps long minute/second hands)."""
    max_r = _hand_content_radius_wm(canvas, pivot_wx, pivot_wy, wm_size)
    min_x = wm_size
    min_y = wm_size
    max_x = -1
    max_y = -1
    found = False
    for yy in range(wm_size):
        for xx in range(wm_size):
            if canvas.getpixel((xx, yy))[3] < 16:
                continue
            if math.hypot(xx - pivot_wx, yy - pivot_wy) > max_r:
                continue
            found = True
            min_x = min(min_x, xx)
            min_y = min(min_y, yy)
            max_x = max(max_x, xx)
            max_y = max(max_y, yy)
    if not found:
        raise ValueError("empty hand crop")
    pad = HAND_PATCH_MARGIN_WM // 2
    min_x = max(0, min_x - pad)
    min_y = max(0, min_y - pad)
    max_x = min(wm_size - 1, max_x + pad)
    max_y = min(wm_size - 1, max_y + pad)
    patch = canvas.crop((min_x, min_y, max_x + 1, max_y + 1))
    return patch, pivot_wx - min_x, pivot_wy - min_y


def _blit_hand_layers(
    zf: zipfile.ZipFile,
    layers: list[ET.Element],
    wm_size: int,
) -> tuple[Image.Image, float, float]:
    if not layers:
        raise ValueError("no hand layers")
    x, y = layer_pivot_wm(layers[0])
    pivot_wx = wm_size / 2.0 + x
    pivot_wy = wm_size / 2.0 + y
    canvas = Image.new("RGBA", (wm_size, wm_size), (0, 0, 0, 0))
    for layer in layers:
        img = apply_watchmaker_layer_tint(_layer_image_for_blit(zf, layer), layer)
        px = int(round(pivot_wx - img.width / 2.0))
        py = int(round(pivot_wy - img.height / 2.0))
        canvas.alpha_composite(img, (px, py))
    return canvas, pivot_wx, pivot_wy


def watchmaker_hand_sprite(
    zf: zipfile.ZipFile,
    layers: list[ET.Element],
    wm_size: int,
) -> tuple[Image.Image, tuple[int, int]]:
    """Extract hand like WatchMaker: cc-blit layer PNG(s), tight crop, scale to desk."""
    canvas, pivot_wx, pivot_wy = _blit_hand_layers(zf, layers, wm_size)
    patch, pivot_px, pivot_py = _tight_hand_crop(canvas, pivot_wx, pivot_wy, wm_size)
    if not patch.getbbox():
        raise ValueError(f"empty hand patch: {layers[0].attrib.get('path')}")
    scale = DESK / wm_size
    nw = max(1, int(round(patch.width * scale)))
    nh = max(1, int(round(patch.height * scale)))
    out = patch.resize((nw, nh), Image.Resampling.LANCZOS)
    px_d = int(round(pivot_px * nw / patch.width))
    py_d = int(round(pivot_py * nh / patch.height))

    w, h = out.size
    if w > HAND_MAX_W or h > HAND_MAX_H:
        f = min(HAND_MAX_W / w, HAND_MAX_H / h)
        nw2 = max(1, int(w * f))
        nh2 = max(1, int(h * f))
        out = out.resize((nw2, nh2), Image.Resampling.LANCZOS)
        px_d = int(round(px_d * nw2 / w))
        py_d = int(round(py_d * nh2 / h))

    px_d = max(0, min(out.width - 1, px_d))
    py_d = max(0, min(out.height - 1, py_d))
    return out, (px_d, py_d)


def hand_length(img: Image.Image, pivot_x: int, pivot_y: int) -> int:
    w, h = img.size
    px, py = pivot_x, pivot_y
    best = 0
    for y in range(h):
        for x in range(w):
            if img.getpixel((x, y))[3] < 16:
                continue
            best = max(best, int(round(math.hypot(x - px, y - py))))
    return max(best, 1)


def dial_from_preview(zf: zipfile.ZipFile, wm_size: int) -> Image.Image | None:
    for name in ("preview.jpg", "preview_dim.jpg"):
        if name not in zf.namelist():
            continue
        prev = Image.open(io.BytesIO(zf.read(name))).convert("RGB")
        if prev.size != (wm_size, wm_size):
            prev = prev.resize((wm_size, wm_size), Image.Resampling.LANCZOS)
        return prev.resize((DESK, DESK), Image.Resampling.LANCZOS)
    return None


def import_watch_file(
    watch_path: Path,
    out_dir: Path,
    slug: str,
    display_name: str,
    force: bool,
) -> ImportResult:
    notes: list[str] = []
    if out_dir.exists() and not force:
        raise SystemExit(f"{out_dir} exists; use --force to overwrite")

    if "watch.xml" not in zipfile.ZipFile(watch_path).namelist():
        raise SystemExit(f"Not a WatchMaker archive (no watch.xml): {watch_path}")

    zf = zipfile.ZipFile(watch_path)
    root = watch_root_from_zip(zf)
    wm_size = wm_canvas_size(zf)

    hands = discover_hands(root)
    if "hour" not in hands or "minute" not in hands:
        raise SystemExit(f"{watch_path.name}: need hour + minute layers ({'{drh}'}/{'{drm}'}) in watch.xml")

    subdials, subdial_skipped = discover_subdials(root)
    for line in subdial_skipped:
        notes.append(f"subdial not mapped: {line}")

    hand_paths = {h.path for h in hands.values()}
    hand_paths.update(sd.path for sd in subdials)
    # Fail before dial compose if hand/subdial PNGs are WatchMaker-protected.
    assert_assets_readable(zf, watch_path.name, hand_paths)
    # Keep full-frame dial JPGs: many faces (D&G, Raggazo) only paint the base dial there;
    # skipping leaves a near-black dial. Live hands still come from extracted PNG layers.
    comp = WatchCompositor(wm_size, hand_paths, strip_full_frame_jpg=False)
    dial_wm = comp.compose_dial(root, zf)
    for skipped in comp.skipped_dial_layers:
        notes.append(f"dial: skipped static layer {skipped}")

    out_dir.mkdir(parents=True, exist_ok=True)
    source_dir = out_dir / "source"
    source_dir.mkdir(parents=True, exist_ok=True)

    dial_gray = dial_wm.convert("L")
    dial_mean = sum(dial_gray.getdata()) / (wm_size * wm_size)
    bbox = dial_wm.getbbox()
    use_preview_fallback = (
        dial_mean < 12
        or bbox is None
        or (bbox[2] - bbox[0]) < wm_size // 4
        or (bbox[3] - bbox[1]) < wm_size // 4
    )
    if use_preview_fallback:
        notes.append(f"composited dial too empty (mean={dial_mean:.0f}); trying preview.jpg fallback")
        prev = dial_from_preview(zf, wm_size)
        if prev:
            prev.save(out_dir / "dial.png")
            notes.append("dial from preview.jpg (may include static hands — verify on device)")
        else:
            dial_wm.convert("RGB").resize((DESK, DESK), Image.Resampling.LANCZOS).save(out_dir / "dial.png")
    else:
        dial_wm.save(source_dir / f"dial_wm{wm_size}.png")
        dial_wm.convert("RGB").resize((DESK, DESK), Image.Resampling.LANCZOS).save(out_dir / "dial.png")

    lengths: dict[str, int] = {}
    hand_entries: dict[str, dict] = {}

    for role in ("hour", "minute", "second"):
        if role not in hands:
            continue
        stack = _main_hand_layers_for_role(root, role)
        sprite, hand_pivot = watchmaker_hand_sprite(zf, stack, wm_size)
        disc = hands[role]
        sprite.save(out_dir / f"{role}.png")
        lengths[role] = hand_length(sprite, hand_pivot[0], hand_pivot[1])
        dial_px, dial_py = dial_anchor_from_layer(disc.layer, wm_size)
        entry: dict = {
            "file": f"{role}.png",
            "length_px": lengths[role],
            "offset_deg": 0,
            "hand_pivot": {"x": hand_pivot[0], "y": hand_pivot[1]},
        }
        if (dial_px, dial_py) != FACE_PIVOT_DESK:
            entry["anchor"] = {"x": dial_px, "y": dial_py}
            notes.append(f"{role} dial pivot ({dial_px}, {dial_py}) — expected main face centre")
        if role == "second":
            entry["optional"] = True
        hand_entries[role] = entry

    manifest: dict = {
        "id": slug,
        "name": display_name,
        "version": 1,
        "pivot": {"x": FACE_PIVOT_DESK[0], "y": FACE_PIVOT_DESK[1]},
        "hands": {
            "hour": hand_entries["hour"],
            "minute": hand_entries["minute"],
        },
        "behaviour": {"show_second_hand": "second" in hand_entries},
    }
    if "second" in hand_entries:
        manifest["hands"]["second"] = hand_entries["second"]

    subdial_manifest: list[dict] = []
    for sd in subdials:
        sprite, hand_pivot = watchmaker_hand_sprite(zf, [sd.layer], wm_size)
        out_name = f"sub_{sd.role}.png"
        sprite.save(out_dir / out_name)
        sx, sy = dial_anchor_from_layer(sd.layer, wm_size)
        subdial_manifest.append(
            {
                "role": sd.role,
                "file": out_name,
                "subdial": {"x": sx, "y": sy},
                "hand_pivot": {"x": hand_pivot[0], "y": hand_pivot[1]},
                "offset_deg": 0,
            }
        )
        notes.append(
            f"subdial {sd.role}: dial pivot desk ({sx}, {sy}), "
            f"sprite pivot {hand_pivot}, asset {sd.path}"
        )

    if subdial_manifest:
        manifest["subdials"] = subdial_manifest
        has_chrono_hand = any(sd.role in ("chrono_second", "chrono_minute") for sd in subdials)
        if has_chrono_hand:
            manifest["behaviour"]["chronograph"] = True

    digital_readout = discover_watchmaker_digital_readout(root, wm_size)
    if digital_readout:
        manifest["behaviour"]["digital_readout"] = digital_readout
        notes.append("live digital readout from WatchMaker text layers")

    weather_cfg = discover_weather_icon(root, wm_size)
    if weather_cfg:
        try:
            for layer in root.findall("Layer"):
                if layer.attrib.get("type") == "image_cond" and "weather_set" in (
                    layer.attrib.get("path") or ""
                ).lower():
                    load_image(zf, layer.attrib["path"]).save(out_dir / weather_cfg["file"])
                    break
        except KeyError:
            notes.append("weather_set sprite missing in .watch archive")
            weather_cfg = None
        if weather_cfg:
            manifest["weather_icon"] = weather_cfg
            notes.append(
                f"weather icon grid {weather_cfg['grid_cols']}x{weather_cfg['grid_rows']} "
                f"at desk ({weather_cfg['x']}, {weather_cfg['y']})"
            )

    with (out_dir / "face.json").open("w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    dest_watch = source_dir / watch_path.name
    if watch_path.resolve() != dest_watch.resolve():
        shutil.copy2(watch_path, dest_watch)

    meta = {
        "source_watch": watch_path.name,
        "watchmaker_name": root.attrib.get("name"),
        "author": root.attrib.get("author"),
        "features": root.attrib.get("features"),
        "wm_canvas": wm_size,
        "hand_assets": {k: v.path for k, v in hands.items()},
        "subdial_assets": {sd.role: sd.path for sd in subdials},
        "notes": notes,
    }
    with (source_dir / "watch-import.json").open("w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
        f.write("\n")

    zf.close()
    return ImportResult(slug=slug, out_dir=out_dir, notes=notes)

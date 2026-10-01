# Watch face authoring specification (Desk Puck)

Version **1.0** — targets ESP32-C3, 240×240 round display, LovyanGFX, analogue hands only.

Product context: [README.md](README.md) · Concept: [watch-faces.md](watch-faces.md)

---

## 1. Design goals

| Goal | Approach |
|------|----------|
| Many distinct styles | One directory per style; add folder → rebuild (no C++ per face). |
| AI-friendly production | Fixed canvas, layer rules, and copy-paste prompts in §6. |
| Desk-quality visuals | Full-colour dial art; anti-aliased hands via alpha sprites. |
| Fits device limits | Build-time conversion; one full-frame dial cache in RAM (~115 KB) while face is active. |

---

## 2. Hardware canvas

| Property | Value |
|----------|--------|
| Panel resolution | **240 × 240** px |
| Visible shape | Circle (~120 px radius); corners are off-screen or black |
| Colour format (runtime) | **RGB565** |
| Safe zone (text / logos) | **Center 200 × 200** circle (≈20 px inset from edge) |
| Hand pivot (default) | **(120, 120)** — geometric centre |
| Preferred export size | **240 × 240** px square (mask applied in firmware or asset) |

Coordinates: **x** right, **y** down, origin top-left.

---

## 3. Layer model (required)

Draw order bottom → top:

```
┌─────────────────────────────────────┐
│  L0  dial.png      Static background│  ← AI art; NO hands, NO moving parts
│  L1  (optional)    complications    │  ← static date window art, logos
│  L2  hour.png      Rotated each tick│  ← alpha sprite, pivot at hub
│  L3  minute.png    Rotated each tick│
│  L4  second.png    Rotated each tick│  ← optional; omit for minute-only mode
│  L5  hub.png       Static cap       │  ← centre jewel / pin (covers pivots)
└─────────────────────────────────────┘
```

**Rule:** Anything that moves with time **must not** appear in `dial.png`.

---

## 4. Asset files (per style folder)

Directory name: **lowercase slug** `[a-z0-9-]` (e.g. `midnight-gold`, `aurora-teal`).

```
src/features/clock/assets/faces/<slug>/
├── face.json          # manifest (required)
├── dial.png           # L0, required
├── hour.png           # L2, required
├── minute.png         # L3, required
├── second.png         # L4, optional
├── hub.png            # L5, optional (recommended)
├── preview.png        # 240×240 for docs / picker UI later (optional)
└── source/            # optional PSD / prompts / notes (not flashed)
```

### 4.1 `dial.png`

- **240 × 240**, sRGB, 8-bit per channel.
- **No hour/minute/second hands**, no motion blur, no “example time”.
- Tick marks, indices, textures, gradients, and **fixed** decorative elements are allowed.
- Prefer **dark or mid tones near centre** under hub (12×12 px) so pivot artefacts are hidden.
- Avoid critical detail in the outer **10 px** ring (bezel clipping).

### 4.2 Hand sprites (`hour.png`, `minute.png`, `second.png`)

- **PNG with alpha** (straight alpha; avoid premultiplied unless build script handles it).
- Hand points **up** (12 o’clock) when angle = 0° in manifest.
- **Pivot** lies on the bottom edge centre of the sprite image (see §5).
- Recommended max bounding box (balance quality vs flash):

| Hand | Typical sprite size (W×H) | Max length (tip from pivot) |
|------|---------------------------|-----------------------------|
| Hour | 40 × 70 | ~52 px |
| Minute | 30 × 95 | ~78 px |
| Second | 8 × 105 | ~88 px (may include counterweight below pivot) |

- Use **solid alpha** along the hand; soft shadow only if it stays readable at 240 px.
- **Do not** embed the hub circle in hand layers if `hub.png` is used.

### 4.3 `hub.png` (optional)

- Small **alpha** overlay, e.g. 24 × 24, centred at (120, 120) when blitted.
- Covers pivot holes and aligns visually with dial art.

---

## 5. Geometry in `face.json`

Example:

```json
{
  "id": "midnight-gold",
  "name": "Midnight Gold",
  "version": 1,
  "canvas": { "width": 240, "height": 240 },
  "pivot": { "x": 120, "y": 120 },
  "hands": {
    "hour": {
      "file": "hour.png",
      "length_px": 52,
      "offset_deg": 0,
      "z": 20
    },
    "minute": {
      "file": "minute.png",
      "length_px": 78,
      "offset_deg": 0,
      "z": 21
    },
    "second": {
      "file": "second.png",
      "length_px": 88,
      "offset_deg": 0,
      "z": 22,
      "optional": true
    }
  },
  "hub": { "file": "hub.png", "z": 30 },
  "behaviour": {
    "show_second_hand": true,
    "tick_hz": 1
  },
  "erase": {
    "mode": "full_redraw",
    "comment": "Use dial cache in RAM; do not colour-erase on photo dials"
  }
}
```

| Field | Meaning |
|-------|---------|
| `offset_deg` | Calibration if art “12 o’clock” is not sprite +Y up |
| `length_px` | Sanity check vs sprite; used by validator |
| `z` | Draw order among hands |
| `erase.mode` | `full_redraw` = blit cached dial before hands each tick (required for raster dials) |

Future: `complications[]` for static overlays with `{ "file", "x", "y" }`.

---

## 6. AI image prompts (templates)

For a **full agent workflow** (system prompt, user task template, pack/upload steps), use [ai-watch-face-agent.md](ai-watch-face-agent.md).

### 6.1 Dial (`dial.png`)

Use as a system or user prompt block; attach reference: “round smartwatch 240×240”.

> Square 240×240 watch **dial face only**, flat orthographic view, no hands, no time display digits unless integrated as static indices. Circular composition centred at 120,120. Rich **\[STYLE\]**: \[e.g. deep navy starfield, brushed copper, art deco enamel\]. Clear hour tick marks at 12/3/6/9. **Leave centre 15px calm/uncluttered** for mechanical pin. No text except optional tiny brand at 6 o’clock. No mockup bezel, no 3D watch case, no drop shadow outside circle. Export PNG, sRGB.

**Negative prompt:** hands, clock hands, minute hand, hour hand, second hand, wrong time, motion blur, Apple Watch mockup, rectangular screen, 3D perspective case.

### 6.2 Hands (separate generations)

Generate **three images** (or one hand sheet then split in an editor):

> Transparent PNG, **single watch hand only**, pointing **straight up**, \[hour / minute / second\] hand, \[STYLE matching dial\], **pivot at bottom centre** of canvas, minimal padding, anti-aliased edges, no background, no drop shadow outside the hand silhouette.

Validate in an editor: rotate 30° steps on checkerboard; pivot should not wobble.

**Tip:** For consistency, use the same LLM session + dial image as reference (“match this dial’s hand style”).

---

## 7. Build pipeline (recommended)

```
src/features/clock/assets/faces/<slug>/*.png + face.json
        │
        ▼
  tools/watch_face_pack.py   (or PlatformIO extra_script)
        │
        ├── validate dimensions, pivot, alpha
        ├── optional: circular mask dial → RGB565
        ├── compress: raw .565 / RLE / QOI (pick one format in firmware)
        └── emit:
              generated/watch_faces_manifest.h   (registry)
              generated/watch_faces_<slug>.cpp   (embedded blobs) OR
              littlefs image (if many large faces)
```

**Registration:** Build scans `assets/faces/*/face.json` and generates `clock_face_count()` entries — **no manual `clock_faces.cpp` edit**.

**Flash budget (rough):**

| Asset | Uncompressed | Typical compressed |
|-------|----------------|---------------------|
| dial RGB565 | 115 KB | 25–60 KB (QOI/RLE/PNG in flash) |
| 3 hand PNGs | 5–30 KB each | keep PNG or pre-multiplied RGBA |

4 MB flash: plan **~5–15** rich faces if embedded, or **more** on LittleFS partition.

---

## 8. Runtime strategy (firmware)

Aligns with current `ClockFeature` lifecycle:

1. **onEnter (face):** Decode `dial` once into a **RAM cache** (115 KB) or LovyanGFX sprite.
2. **Each second:** Blit cache → draw rotated hand sprites → blit hub.
3. **onExit:** Release cache.

**Do not** use single-colour hand erase on photographic dials (current procedural `erase_color` trick).

**Image-based hands:** **Yes, recommended** — separate alpha PNGs rotated with LovyanGFX `pushImageRotateZoom` (or equivalent). Avoid baking hands into the dial.

---

## 9. Quality checklist (before commit)

- [ ] `dial.png` has **no hands** at any rotation (visual inspect).
- [ ] All hand PNGs have **alpha**, pivot at bottom centre.
- [ ] Rotate hands ±90° in editor — pivot stable at dial centre when overlaid on dial.
- [ ] Legible at arm’s length on real hardware (upload once, review).
- [ ] `face.json` `id` matches folder slug.
- [ ] Validator script passes (when implemented).

---

## 10. Procedural vs asset faces

| | Procedural (`faces/*.cpp`) | Asset (`assets/faces/`) |
|--|--------------------------------------|-------------------------------|
| Best for | Bootstrapping, ultra-low flash | AI / designer art, many styles |
| Hands | Vectors / wide lines | PNG sprites |
| Add new style | C++ + recompile | Folder + rebuild |
| RAM | Lower | ~115 KB dial cache while active |

Both can coexist in one registry with a `type: "procedural" | "asset"` field in manifest.

---

## 11. Alternatives considered

| Idea | Verdict |
|------|---------|
| Full dial + hands in one AI image per minute | Rejected — 1440× day states or jittery AI hands |
| Vector hands from LLM JSON | Good for minimal flash; lower “luxury” look |
| Runtime PNG decode every second | Rejected — CPU + flash wear; decode once |
| 115 KB sprite always + double buffer | Rejected on C3 — one buffer enough if redraw from cache |
| LVGL | Out of scope per AGENTS.md |

---

## 12. Next implementation steps (firmware)

1. Copy `assets/faces/_template/` to a new slug with placeholder manifest.
2. Implement `AssetClockFace` loader + RAM dial cache.
3. Add `tools/watch_face_pack.py` validator + code generation.
4. Optional: migrate **Midnight** to an asset pack when a dial is ready.

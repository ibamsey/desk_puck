# Asset-based watch faces (concept)

High-quality styles for the clock feature: **many looks**, **AI-friendly authoring**, **easy loading** without a new C++ module per face.

## Problem

Procedural drawing (`faces/*.cpp` in this feature) is fine for bring-up but caps visual quality. We want desk-object polish—textures, illustration, art deco, starfields—at a scale where **you add folders, not firmware modules**.

## Recommended approach

| Layer | What | How it is made |
|-------|------|----------------|
| **Dial** | Static background (ticks, art, indices) | LLM / designer → `dial.png` (**no hands**) |
| **Hands** | Hour, minute, second | Separate **alpha PNG sprites**, rotated each second in code |
| **Hub** | Centre pin / jewel | Optional small `hub.png` on top |

**Do not** bake moving hands into the dial image. **Do not** rely on single-colour “erase” over photographic dials (current procedural trick). Asset faces keep one **static layer** in RAM (dial + hour + minute + hub, rebuilt from flash on the hour/minute change). The second hand and chrono subdials are restored by blitting only their rotated dirty rects from that frozen layer, then redrawn.

**Image-based hands:** Yes—separate PNGs with transparency and a fixed pivot at the dial centre. LovyanGFX can rotate sprites; hands stay sharp and match the dial style when generated with the dial as reference.

## Folder layout (authoring)

One slug folder per style under `assets/faces/`:

```
src/features/clock/assets/faces/<slug>/
├── face.json       # manifest (pivot, hand files, behaviour)
├── dial.png
├── hour.png
├── minute.png
├── second.png      # optional
├── hub.png         # optional
└── source/         # prompts, PSD — not flashed
```

Full file rules, sizes, and `face.json` schema: [authoring-spec.md](authoring-spec.md).

## Build and runtime pipeline

```text
assets/faces/<slug>/  →  tools/watch_face_pack.py  →  generated/ registry + blobs
                                      │
                                      ▼
                            AssetClockFace (planned)
                                      │
                                      ▼
                            ClockFeature (existing shell)
```

- **Build:** Validate PNGs, convert dial to flash-friendly RGB565 / QOI / RLE, auto-generate face registry (no hand-editing `clock_faces.cpp`).
- **Runtime:** On face enter, compose a **~115 KB static layer** (dial from flash + hour + minute + hub). Smooth second ticks restore only the hand’s dirty rect from that layer, then rotate the second (and chrono) sprites. Hour/minute are not redrawn between wall minutes.
- **Flash:** Roughly **5–15** rich embedded faces in 4 MB, or more via LittleFS if needed.

## AI workflow (short)

1. Prompt for **dial only** (240×240, no hands) — templates in [authoring-spec.md §6](authoring-spec.md#6-ai-image-prompts-templates).
2. Prompt for **three transparent hands** (12 o’clock, pivot bottom-centre), using dial as style reference.
3. Quick QA: pivot wobble check, overlay on dial, hardware glance.
4. Drop folder, run pack script (when implemented), rebuild, upload.

## Coexistence with procedural faces

| | Procedural | Asset pack |
|--|------------|------------|
| Location | `faces/` (procedural C++) | `assets/faces/` (PNG packs) |
| Best for | Demos, tiny flash | AI / designer art |
| Registry | `clock_faces.cpp` today | Generated manifest (planned) |

Both can share one registry with `type: procedural | asset` in manifest.

## Implementation status

| Step | Status |
|------|--------|
| Procedural Midnight | Shipped (boot default) |
| `assets/faces/demo/` example pack | Shipped |
| `tools/watch_face_pack.py` + PlatformIO pre-build | Shipped |
| `AssetFaceRuntime` static layer + dirty-rect restore | Shipped |
| Unified registry (procedural + asset) | Shipped |
| LittleFS delivery | Deferred |

Binary layout: [pack-format.md](pack-format.md). Generated output: `generated/`.

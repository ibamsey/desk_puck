# AI agent prompt — new asset watch face

Use this document when an **AI coding agent** (or human + image model) should add a **new shipped watch face** to Desk Puck. Full rules: [authoring-spec.md](authoring-spec.md). Binary layout: [pack-format.md](pack-format.md).

---

## Copy-paste: agent system prompt

```text
You are building a new analogue watch face for Desk Puck (ESP32-C3, 240×240 round GC9A01, LovyanGFX).

GOAL
Create a complete asset pack at:
  src/features/clock/assets/faces/<slug>/
where <slug> is lowercase [a-z0-9-] and matches face.json "id".

DELIVERABLES (slug folder root — required unless noted)
  face.json       manifest (see schema below)
  dial.png        240×240 RGB, static dial ONLY — no hands, no showing a specific time
  hour.png        RGBA, hand pointing straight up (12 o'clock); pivot at bottom-centre of image
  minute.png      RGBA, same rules, longer than hour
  second.png      RGBA, optional but recommended; set "optional": true in manifest
  hub.png         RGBA ~24×24, centre cap covering pivot (recommended)
  source/         optional: save generation prompts, raw JPGs, notes (never flashed)

HARD RULES
1. dial.png must NOT contain hour, minute, or second hands or motion blur.
2. Hand sprites rotate around display centre (120, 120). Pivot = bottom edge centre of each hand PNG.
3. Keep centre ~15px of dial calm/dark for hub overlay.
4. Max hand bounds (packer enforces): hour ≤48×110, minute ≤48×110, second ≤48×110.
5. id in face.json MUST equal folder name <slug>.
6. Do NOT edit C++ or clock_faces.cpp — registration is automatic via watch_face_pack.py.

face.json MINIMAL SCHEMA (extend only with documented fields)
{
  "id": "<slug>",
  "name": "<Display Name>",
  "version": 1,
  "pivot": { "x": 120, "y": 120 },
  "hands": {
    "hour":   { "file": "hour.png",   "length_px": 52, "offset_deg": 0 },
    "minute": { "file": "minute.png", "length_px": 78, "offset_deg": 0 },
    "second": { "file": "second.png", "length_px": 88, "offset_deg": 0, "optional": true }
  },
  "hub": { "file": "hub.png" }
}

WORKFLOW
1. Choose <slug> and <Display Name>. Confirm slug is not already used under assets/faces/.
2. Generate or draw assets (image model prompts below). Post-process: exact sizes, alpha, pivot check.
3. Write face.json. Place PNGs at slug root.
4. From repo root run: python tools/watch_face_pack.py
   - Must print "Packing <slug>..." and report face count without errors.
5. Run: pio run -t upload (unless user asked build-only).
6. Document in source/README.md: style notes and how art was produced.

QUALITY CHECK (before finishing)
- Overlay hour/minute/second on dial in an editor; rotate 30° steps — pivot stays at dial centre.
- dial has no hands at any rotation.
- Legible tick marks at arm's length (240px canvas).

If JPG→PNG extraction is unreliable, prefer separate PNG hand generations or a small tools/faces/<slug>/generate.py (see steampunk example). Only watch_face_pack.py is required for firmware.
```

---

## Copy-paste: user task message (fill in brackets)

```text
Create a new Desk Puck asset watch face.

Slug: [e.g. ocean-depths]
Display name: [e.g. Ocean Depths]
Style: [2–4 sentences — palette, era, mood, materials]

Requirements:
- [ ] Second hand: yes / no
- [ ] Roman numerals / indices / none on dial
- [ ] Any fixed text (brand at 6 o'clock): [none / text]

Use the agent system prompt in src/features/clock/docs/ai-watch-face-agent.md.
Save outputs under src/features/clock/assets/faces/[slug]/.
Run watch_face_pack.py and pio run -t upload when done.
```

---

## Image model prompts (per layer)

Replace `[STYLE]` with the same phrase in every prompt so hands match the dial.

### Dial — `dial.png`

**Prompt:**

> Square 240×240 watch dial face only, flat orthographic top-down view, circular composition centred at pixel 120,120. Style: **[STYLE]**. Clear hour markers (ticks or indices at 12, 3, 6, 9). Rich texture and colour but **empty centre 15px** for a mechanical pin. No clock hands, no minute hand, no hour hand, no second hand, no digital time, no hands showing 10:10 or any time. No smartwatch mockup, no 3D watch case, no rectangular screen, no drop shadow outside the circle. PNG, sRGB, sharp edges suitable for a small round display.

**Negative:** hands, clock hands, wrong time, motion blur, Apple Watch, bezel mockup, perspective case, cropped circle.

### Hour hand — `hour.png`

> Transparent PNG, single **hour** watch hand only, pointing **straight up**, style **[STYLE]** matching the dial, shorter and wider than a minute hand, pivot at **bottom centre** of canvas with 1px padding below pivot, anti-aliased alpha edges, no background, no hub disk, no drop shadow outside silhouette.

### Minute hand — `minute.png`

> Transparent PNG, single **minute** watch hand only, pointing **straight up**, style **[STYLE]**, longer and slightly thinner than the hour hand, pivot at **bottom centre**, minimal padding, alpha channel, no background.

### Second hand — `second.png` (optional)

> Transparent PNG, single **second** watch hand only, pointing **straight up**, style **[STYLE]**, very thin, may include small counterweight below pivot, pivot at **bottom centre**, alpha, no background.

### Hub — `hub.png`

> Transparent PNG, small round watch centre cap or jewel (~24×24), style **[STYLE]**, alpha, no hand attached, top-down, matches dial centre ornament.

**Consistency tip:** In the same chat/session, attach `dial.png` and ask each hand to “match this dial’s material and line weight.”

---

## Post-generation checklist (agent)

| Step | Action |
|------|--------|
| Size | `dial.png` exactly 240×240 RGB; hands within max bounds in authoring-spec §4.2 |
| Pivot | Bottom-centre of hand bbox is rotation anchor; extra transparent rows below pivot are OK |
| Alpha | Hands and hub use straight alpha PNG |
| Manifest | `id` === folder name; `length_px` roughly matches tip distance from pivot |
| Pack | `python tools/watch_face_pack.py` from repo root |
| Flash | `pio run -t upload` (COM5 per platformio.ini unless user says otherwise) |

---

## Example `face.json` (shipped shape)

```json
{
  "id": "ocean-depths",
  "name": "Ocean Depths",
  "version": 1,
  "pivot": { "x": 120, "y": 120 },
  "hands": {
    "hour": { "file": "hour.png", "length_px": 52, "offset_deg": 0 },
    "minute": { "file": "minute.png", "length_px": 78, "offset_deg": 0 },
    "second": {
      "file": "second.png",
      "length_px": 88,
      "offset_deg": 0,
      "optional": true
    }
  },
  "hub": { "file": "hub.png" }
}
```

---

## Repo paths (quick reference)

| What | Path |
|------|------|
| New face folder | `src/features/clock/assets/faces/<slug>/` |
| Template notes | `src/features/clock/assets/faces/_template/README.md` |
| Packer | `tools/watch_face_pack.py` or `src/features/clock/tools/watch_face_pack.py` |
| Generated firmware blobs | `src/features/clock/generated/` (do not hand-edit) |
| Optional procedural example | `src/features/clock/tools/faces/steampunk/generate.py` |

After pack, the face appears in the app when the user swipes **up/down** on the clock screen; order is procedural faces first, then asset faces sorted by slug.

---

## When not to use this prompt

- **Procedural faces** (vector C++ in `faces/face_*.cpp`) — different path; see [watch-faces.md](watch-faces.md).
- **Editing an existing slug** — same deliverables, overwrite PNGs in that folder only; re-run pack + upload.

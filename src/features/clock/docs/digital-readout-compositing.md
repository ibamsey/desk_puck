# Live digital readout (compositing contract)

Wall time and date strings that update in firmware must follow the same **layer order** on asset and procedural faces so the second hand stays smooth and nothing flickers.

**Order (bottom → top):** dial → **digital text** → hour / minute / hub → **second hand** (and chrono subdials on asset faces).

Digital text is **never** drawn on the panel after the hands. It is baked into the off-screen **static** buffer, then only **readout band** patches (regions that exclude the second-hand dirty rect) refresh from that buffer each frame.

---

## Asset faces (`assets/faces/<slug>/`)

1. **`dial.png`** — no hands, no live digits (leave a calm centre if text sits there).
2. **`face.json`** — enable compositor:

```json
"behaviour": {
  "show_second_hand": true,
  "digital_readout": true
}
```

Or with layout and colours (recommended for new faces):

```json
"behaviour": {
  "show_second_hand": true,
  "digital_readout": {
    "time_y": 108,
    "date_y": 132,
    "time_color": "0xFFFF",
    "date_color": "0x9CD3",
    "bg_color": "0x1084",
    "time_text_size": 2,
    "date_text_size": 1
  }
}
```

3. **Authoring** — pick `time_y` / `date_y` on a blank dial in an editor; keep hub area clear. Reference: [wayfinder](../assets/faces/wayfinder/face.json).

4. **Build** — `python src/features/clock/tools/watch_face_pack.py` (or `pio run`) emits `AssetDigitalReadoutMeta` in `generated/watch_face_manifest.h`. No C++ edits.

5. **Runtime** — `AssetFaceRuntime::present_digital_compositor()` (auto when `digital_readout` flag is set).

---

## Procedural faces (`faces/face_*.cpp`)

1. Set `use_static_compositor = true` on the `ClockFace`.
2. Implement `draw_digital_overlay` (text only; `clear_background = false`).
3. Do **not** draw digits in `draw_static` — the procedural compositor calls `draw_digital_overlay` on the static layer **before** hour/minute/hub.

Example: [face_classic_digital.cpp](../faces/face_classic_digital.cpp).

---

## Do not

| Anti-pattern | Why |
|--------------|-----|
| Live digits in `dial.png` | Seconds cannot tick; wastes flash |
| `draw_digital_overlay` on the panel after hands | Wrong z-order; flicker |
| Solid `fillRect` behind text every frame | Rectangles on gradient dials |
| Splitting the second-hand bbox and drawing the hand into each piece | Ghost lines / boxes (see Classic Digital history) |

---

## Related docs

- [authoring-spec.md](authoring-spec.md) — layer model and `face.json`
- [ai-watch-face-agent.md](ai-watch-face-agent.md) — agent checklist
- [assets/README.md](../assets/README.md) — folder layout

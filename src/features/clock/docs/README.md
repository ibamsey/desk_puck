# Feature: Clock (home screen)

| Field | Value |
|-------|--------|
| Status | in progress |
| Depends on | SNTP when WiFi is up ([wall_clock.cpp](../../../time/wall_clock.cpp)); compile-time fallback until sync |
| Primary input | glance; swipe up/down to change watch face |

## Summary

On power-on and during normal idle use, Desk Puck shows a **clear, beautiful clock**—the default face of the gadget. Everything else is secondary modes the user can reach through touch or future navigation.

## Why it’s fun

A round puck on the desk is naturally a **timepiece**: no rectangle pretending to be a phone. Typography, subtle motion (e.g. a soft second tick), and a calm palette make it an object you want on the desk even when you are not “using” it.

## Behaviour

### Entry and exit

- **Entry:** Boot completes → clock is the first full-screen UI.
- **Exit:** Swipe left/right to other features; swipe down from other features returns home ([ADR-01](../../../adr/ADR-01-touch-navigation-and-app-shell.md)). Clock remains **home** (feature index 0).

### On-screen

- **Primary:** Analogue watch faces with hour, minute, and second hands.
- **Watch faces (shipped):** Procedural **Midnight** (boot default); asset **Demo**, **Aurora**, **Steampunk**, **Classic Chrono**, plus any other slugs under `assets/faces/`. Swipe up/down cycles faces.
- **Chronograph:** On **Classic Chrono**, tap start/stop, double-tap reset — [chronograph.md](chronograph.md).
- **More asset faces:** Add `assets/faces/<slug>/`, run `python tools/watch_face_pack.py` from repo root (or rebuild); see [watch-faces.md](watch-faces.md), [authoring-spec.md](authoring-spec.md), [assets/README.md](../assets/README.md).
- **Layout:** Centred inside the round safe area (~98 px dial radius); avoid critical detail in the outer ~20 px.
- **Refresh:** Background on face change; hands updated each second (procedural faces use colour erase; asset faces will redraw from a dial cache).

### Interaction

- **Swipe up / swipe down:** Next / previous watch face (while on clock).
- **Swipe left / right:** Other app features (e.g. patterns).
- **Later:** Tap for 12/24 h, brightness, timezone; NTP time sync.

### Edge cases

- First boot with no network: **firmware build timestamp** + `CLOCK_UTC_OFFSET_SEC` in `config.h`.
- Day rollover, DST: when a proper time source exists.
- Display sleep: policy TBD.

## Non-goals (v1)

- World clocks, alarms, timers (separate feature docs).
- Full calendar or meeting integration.
- LVGL unless a future ADR adopts it.

## Technical notes

| Layer | Location |
|-------|----------|
| App shell feature | `clock_feature.cpp` (this folder) |
| Procedural faces | `faces/`, registry in `clock_faces.cpp` |
| Asset runtime | `asset_face.cpp`, pack format [pack-format.md](pack-format.md) |
| Draw helpers | `clock_draw.cpp` |
| Time | `wall_time.cpp` |
| Authoring | `assets/faces/<slug>/` → `generated/` |

## Docs in this folder

| File | Purpose |
|------|---------|
| [watch-faces.md](watch-faces.md) | Concept: AI dials, sprite hands, folder pipeline |
| [authoring-spec.md](authoring-spec.md) | Canvas, layers, manifest, prompts, checklist |
| [ai-watch-face-agent.md](ai-watch-face-agent.md) | Copy-paste agent + image-model prompts for new faces |
| [pack-format.md](pack-format.md) | Embedded binary blob layout (v1) |
| [chronograph.md](chronograph.md) | Stopwatch manifest + tap gestures |

## Open questions

- [ ] 12 h vs 24 h default?
- [ ] Persist last selected watch face across reboot?
- [ ] Boot splash vs straight to clock?
- [ ] Always-on vs display timeout?

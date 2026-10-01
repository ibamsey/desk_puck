# Desk Puck — overview

Greenfield firmware scaffold for the JCZN **ESP32-2424S012C** round touch module.

Product feature specs live in [features/](features/README.md) (clock-first desk gadget). Architecture decisions: [adr/](../adr/README.md).

**Runtime:** PlatformIO + Arduino on ESP32 (FreeRTOS under the hood); cooperative `setup()` / `loop()` — [ADR-00](../adr/ADR-00-arduino-runtime-and-freertos.md).

## Data flow

```
main.cpp
  ├── Display::begin()
  ├── TouchInput::begin() / poll()  → InputEvent
  └── AppShell::handleInput() / tick()
        ├── navigation (swipe carousel, swipe down = home)
        └── active Feature::onTick / onDraw
```

Details: [ADR-01](../adr/ADR-01-touch-navigation-and-app-shell.md).

There is no network stack in the starter build. Add modules under `src/` when the product needs WiFi, MQTT, HTTP, etc.

## Extension checklist

1. **New feature** — Add `src/features/<name>/` ([layout](../src/features/README.md)), register in `app_shell.cpp`, extend `platformio.ini` `build_src_filter` and `build_flags` include path.
2. **Driver** — Keep `LGFX_config.h` as the single panel entry; draw via `Display::gfx()`.
3. **Input** — Route all touch through `touch_input.cpp`; shell owns global swipes.
4. **State** — Per-feature members; avoid globals beyond hardware singletons.

## Coordinate system

Round panel is **240×240** Cartesian pixels. Origin top-left. The starter draws a decorative circle inset from the bezel; place primary content inside ~radius 100 px for readability.

## Relationship to HomePuck

| | HomePuck | Desk Puck |
|---|----------|-----------|
| Hardware | ESP32-2424S012C | Same |
| Display driver | `LGFX_config.h` | Copied baseline |
| Application | Marine / SignalK | User-defined |
| Repo | github.com/ibamsey/HomePuck | This repo |

Restore HomePuck firmware from the HomePuck repo and tag `homepuck-2026-10-01` if you reuse the same physical puck for boat work.

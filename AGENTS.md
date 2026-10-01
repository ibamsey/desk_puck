# Agent Guidelines for Desk Puck

Guidance for AI assistants working on **desk_puck** — a greenfield app on the ESP32-2424S012 round display.

---

## Project summary

**Desk Puck** is an **ESP32-C3** module with a 240×240 **GC9A01** round panel and **CST816S** touch. UI is **LovyanGFX** in `src/display/`. This is **not** HomePuck: do not copy boat WiFi, SignalK, or marine screens unless the user explicitly asks.

Hardware twin (proven pin map): [HomePuck](../HomePuck) — use its [docs/hardware.md](../HomePuck/docs/hardware.md) for deep reference only.

---

## Source layout

```
src/
├── main.cpp, LGFX_config.h
├── app/, display/, input/     Shell + shared UI ([ADR-01](adr/ADR-01-touch-navigation-and-app-shell.md))
├── wifi/, time/               Shared services
└── features/                  One folder per product feature ([features/README.md](src/features/README.md))
    ├── clock/                 code, include/, assets/, tools/, docs/, generated/
    ├── diary/
    └── patterns/

include/                       Cross-cutting only (config.h, feature.h, app_shell.h, …)
```

| Task | Do this | NOT this |
|------|---------|----------|
| Runtime / OS | Arduino `loop()` + app shell ([ADR-00](adr/ADR-00-arduino-runtime-and-freertos.md)) | Pure ESP-IDF port, Zephyr, app LVGL unless ADR |
| New shell feature | `src/features/<name>/` + `include/<name>/` inside it + register in `app_shell.cpp` | LVGL / EEZ / `src/ui/` |
| Pin or panel change | `config.h`, `LGFX_config.h`, `platformio.ini` build_flags | Hard-coded GPIO in many files |
| Network feature | New `src/wifi/` or `src/net/` module when requested | Port HomePuck stack by default |
| Touch gestures | `src/input/touch_input.cpp` only | CST816S from feature code |

---

## ESP32-C3 constraints

| Item | Note |
|------|------|
| **RAM** | ~320 KB total, **no PSRAM** |
| **Display** | Full-frame RGB565 buffer ≈ 115 KB if you allocate a sprite |
| **Loop** | Keep `loop()` responsive; use `delay(1–10)` or `yield()` if doing heavy work |

Before adding WiFi + TLS + large buffers, estimate heap (`ESP.getFreeHeap()` in serial).

---

## Touch (CST816S)

After `touch.begin()`, always call **`touch.disable_auto_sleep()`** or gestures stop after idle (same as HomePuck).

Gesture IDs come from the [CST816S](https://github.com/fbiego/CST816S) library (`SWIPE_LEFT`, etc.). Debounce with `TOUCH_GESTURE_DEBOUNCE_MS` in `config.h`.

---

## Build and flash

```powershell
cd C:\Users\ian\projects\desk_puck
pio run -t upload -t monitor
```

Upload port: **COM5** in `platformio.ini` (dev machine default; verify with `pio device list`).

After firmware changes, **always run upload** (`pio run -t upload`) so the puck on the desk stays in sync—unless the user says build-only or the port is unavailable.

---

## Documentation

| Doc | Purpose |
|-----|---------|
| [adr/README.md](adr/README.md) | ADRs: [ADR-00](adr/ADR-00-arduino-runtime-and-freertos.md) runtime, [ADR-01](adr/ADR-01-touch-navigation-and-app-shell.md) touch shell |
| [docs/overview.md](docs/overview.md) | Architecture and extension points |
| [docs/hardware.md](docs/hardware.md) | Board, pins, USB, flash |
| [docs/bringup.md](docs/bringup.md) | First flash and serial checks |

When behaviour stabilises, update `APP_VERSION` in `config.h` and note breaking changes in README.

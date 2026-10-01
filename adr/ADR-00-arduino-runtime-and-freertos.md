# ADR-00: Arduino runtime and FreeRTOS

## Status

Accepted

## Date

2026-10-01

## Context

Desk Puck firmware targets **ESP32-C3** on the ESP32-2424S012 round module. We need a clear answer to “what framework do we use?” — especially whether to adopt a separate **RTOS-centric application framework** (pure ESP-IDF, Zephyr, etc.) or a **UI/runtime stack** (LVGL, EEZ) on top of the OS.

The repo already builds with **PlatformIO** and **`framework = arduino`** ([platformio.ini](../platformio.ini)). Application code uses **`setup()` / `loop()`** and C++ libraries (**LovyanGFX**, **CST816S**). RAM is ~320 KB with **no PSRAM**; heavy middleware competes with display buffers ([AGENTS.md](../AGENTS.md)).

Product architecture for screens and touch is defined separately in [ADR-01](ADR-01-touch-navigation-and-app-shell.md). This ADR fixes the **platform and concurrency model** underneath that shell.

## Decision

### 1. Build and SDK stack (unchanged)

| Piece | Choice |
|-------|--------|
| Build system | **PlatformIO** |
| Platform | **espressif32** (ESP-IDF-based toolchain) |
| Application framework | **Arduino** (`framework = arduino` in `platformio.ini`) |
| Language | **C++** for application code; IDF/C as pulled in by the platform |

We do **not** migrate the project to pure ESP-IDF (`framework = espidf`) or another OS (Zephyr, etc.) unless a future ADR supersedes this one.

### 2. FreeRTOS is already present — app code stays cooperative first

Arduino-ESP32 is implemented on **ESP-IDF**, which runs **FreeRTOS**. System services (WiFi stack, TCP, timers, USB CDC) use tasks and IDF APIs whether or not the sketch creates tasks.

**Application model for Desk Puck:**

- Use **`setup()` once**, then a single **`loop()`** path: poll input → tick app shell → draw when dirty → short **`delay()` / `yield()`** so the idle task and watchdog stay healthy.
- Do **not** introduce app **`xTaskCreate`** / queues in the starter or clock feature unless a concrete need appears (see follow-ups).

Mental model: **one logical “UI thread”** owned by `loop()`; FreeRTOS exists for the platform, not as our primary app structure.

### 3. “Framework” = our modules, not a third-party app OS

The intentional application layers are:

1. **Runtime** — Arduino on ESP32 (FreeRTOS underneath).
2. **Shell and features** — [ADR-01](ADR-01-touch-navigation-and-app-shell.md) (`app_shell`, `input`, per-feature folders under `src/`).
3. **Rendering** — LovyanGFX via `LGFX_config.h` and `src/display/`.
4. **Libraries** — Arduino-style deps in `platformio.ini` `lib_deps`.

We do **not** adopt LVGL, EEZ Studio, or a generic “RTOS application framework” as the default UI or navigation layer.

### 4. Concurrency rules when we add network or background work

When WiFi, HTTP, MQTT, NTP, or OTA land:

- Prefer **non-blocking** APIs and **short work** in `loop()` first.
- If blocking or lengthy work is required, add **dedicated FreeRTOS tasks** (IDF or Arduino-compatible patterns) that communicate with the shell via **queues or event flags** — never call LovyanGFX draw from multiple tasks without an explicit locking ADR.
- **Drawing and shell state updates** remain on the `loop()` / shell path unless superseded.

### 5. What belongs in `platformio.ini`

- Board: **esp32-c3-devkitm-1** (module match for this puck).
- USB CDC on boot flags as already configured for serial over native USB.
- New capabilities add **`lib_deps`** or `src/` modules; switching to `framework = espidf` requires a new ADR.

## Consequences

### Positive

- Matches current repo, HomePuck hardware path, and LovyanGFX/CST816S ecosystem.
- Contributors get familiar Arduino entry (`main.cpp`) without learning IDF bootstrapping first.
- Avoids LVGL RAM cost and framework lock-in for a clock-first desk object.
- FreeRTOS remains available when network features need real background tasks.

### Negative / trade-offs

- Arduino abstraction hides some IDF details (logging, Kconfig); deep debugging may still require IDF docs.
- Cooperative `loop()` can stall if code blocks (TLS, long `delay`); discipline and later task split are required.
- Pure-IDF teams may prefer `espidf`; this project explicitly chooses Arduino for velocity and library fit.

### Follow-ups

- New ADR when adding **first persistent background task** (pattern: net task → queue → shell).
- New ADR if **LVGL or sprite full-frame buffer** strategy is adopted (heap impact).
- NTP/time sync module under `src/net/` or `src/wifi/` when clock needs network time.

## Alternatives considered

| Option | Why not (or deferred) |
|--------|------------------------|
| Pure **ESP-IDF** (`framework = espidf`) | Valid for large products; rewrites init, complicates LovyanGFX/CST816S bring-up; defer unless Arduino limits us. |
| **Zephyr** / non-Espressif RTOS | Wrong ecosystem for this board and existing pin/driver work. |
| **LVGL** / EEZ as app UI | RAM and complexity; AGENTS steer direct LovyanGFX modules; revisit only with ADR. |
| **MicroPython** | Easier scripting, weaker real-time/display performance for tight round UI. |
| App-wide **multi-task** design from day one | Premature without network; ADR-01 shell fits single `loop()` tick model. |

## References

- [platformio.ini](../platformio.ini) — `framework = arduino`, board, `lib_deps`
- [src/main.cpp](../src/main.cpp) — `setup()` / `loop()`
- [ADR-01](ADR-01-touch-navigation-and-app-shell.md) — app shell on top of this runtime
- [docs/overview.md](../docs/overview.md) — extension checklist
- [Espressif Arduino core](https://github.com/espressif/arduino-esp32) — Arduino on IDF/FreeRTOS

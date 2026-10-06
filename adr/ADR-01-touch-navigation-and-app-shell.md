# ADR-01: Touch navigation and app shell

## Status

Accepted

## Date

2026-10-01

## Context

Desk Puck is a round **240×240** touch gadget whose default experience is a **clock** ([src/features/clock/docs/README.md](../src/features/clock/docs/README.md)), with additional modes described as separate features over time. The **CST816S** controller exposes **gestures** (swipes, tap, double tap) rather than a continuous touch stream suited to free-form drawing; firmware already polls gestures in `main.cpp` with debouncing (`TOUCH_GESTURE_DEBOUNCE_MS` in `config.h`).

We need a single architectural story for:

1. **Navigating between features** at the product “top level” using touch.
2. **Interacting inside a feature** (e.g. toggle format, dismiss detail) without those gestures hijacking global navigation.
3. **Keeping `loop()` responsive** on ESP32-C3 with limited RAM and no PSRAM ([AGENTS.md](../AGENTS.md)).

Feature documents must not each invent their own navigation model; implementation must stay LovyanGFX-based under `src/display/` unless a future ADR says otherwise. Platform and concurrency: [ADR-00](ADR-00-arduino-runtime-and-freertos.md).

## Decision

### 1. App shell owns global navigation

Introduce an **app shell** (coordinator) that:

- Maintains a **registry of features** (ordered list; clock is index `0` and the **home** feature).
- Tracks the **active feature** and forwards lifecycle calls: `onEnter`, `onExit`, `onTick`, `onDraw`.
- Owns **global navigation gestures** so feature code does not call `CST816S` directly for shell-level behaviour.

Suggested layout (names may vary; boundaries must not):

```
src/
├── main.cpp              setup, loop: poll hardware, call shell.tick()
├── app/
│   └── app_shell.cpp/h   feature registry, active feature, nav policy
├── input/
│   └── touch_input.cpp/h CST816S poll, debounce, classify events
├── display/
│   └── display.cpp/h     LovyanGFX init, shared draw helpers
├── clock/                clock feature (shell + faces, …)
└── <feature>/            one folder per shell feature (clock, diary, …)
```

`main.cpp` stays thin: init display and touch, construct shell, `loop()` → `touch_input.update()` → `app_shell.tick(now)`.

### 2. Two-layer input routing

All touch events flow **input → shell → (optional) feature**:

| Layer | Responsibility |
|-------|----------------|
| **Touch input** | Read CST816S when available; apply debounce; emit a small **InputEvent** enum (swipe directions, tap, double tap, none). |
| **App shell** | Apply **navigation policy** (below). Events not consumed by navigation are passed to the active feature. |
| **Feature** | Handle **local interaction** only: taps, double taps, or gestures explicitly registered for that screen. |

Features **must not** read the touch controller directly once the input module exists.

### 3. Default navigation policy (v1)

Global behaviour (shell consumes these unless a feature **captures** navigation — see below):

| Gesture | Shell action |
|---------|----------------|
| Swipe left | Next feature (wrap) |
| Swipe right | Previous feature (wrap) |
| Swipe up | Reserved (e.g. settings overlay or feature palette); v1 may no-op |
| Swipe down | **Go home** → active feature = clock (index 0) |
| Tap | Pass to active feature |
| Double tap | Pass to active feature (or shell shortcut if feature does not handle) |

**Home** is always the clock feature. Returning home does not reset persisted feature settings unless the feature chooses to on `onEnter`.

**Feature capture (escape hatch):** A feature may set `capturesNavigation = true` while a modal or sub-flow is open (e.g. picker). While captured, swipes are delivered to the feature only; swiping down still may mean “cancel” if the feature handles it, or shell defines double-down — document in the feature spec before implementing.

Debounce remains global in the input layer so rapid swipes do not skip multiple features.

### 4. Feature module contract

Each feature is a **plain module** (no LVGL), implementing a shared interface along these lines:

- **`id` / `name`** — debug and serial logging.
- **`onEnter()`** — start timers, request full redraw.
- **`onExit()`** — stop animations, release capture flags.
- **`onTick(now_ms)`** — periodic updates (clock every second/minute, etc.).
- **`onDraw(lgfx)`** — draw the full screen for that feature (or dirty regions if optimized later).
- **`onInput(event)`** — return `true` if handled; shell skips default nav only when policy says feature gets first refusal (taps always; swipes when `capturesNavigation`).

Registration happens once at boot in the shell (static table or explicit register calls). Order in the table defines **carousel order** for swipe left/right.

### 5. Drawing and tick timing

- **Tick:** Shell calls `onTick` on the active feature every loop or on a feature-specific interval; clock uses wall-clock delta, not only `DISPLAY_UPDATE_INTERVAL_MS`.
- **Draw:** Shell calls `onDraw` when the feature marks **dirty** or on enter. Avoid full-frame redraw every 100 ms unless needed (animation or clock seconds).
- **Sprites:** Full-frame RGB565 buffer ≈ 115 KB; prefer direct draw or partial updates until an ADR approves a sprite strategy.

### 6. Mapping to product docs

| Product doc | Architecture |
|-------------|--------------|
| [clock/docs/README.md](../src/features/clock/docs/README.md) | Feature index `0`, default on boot and on swipe-down home |
| Future `docs/features/<name>/` | Additional registry entries; entry/exit in feature spec must align with shell policy |

When a feature spec conflicts with this ADR, update the spec or supersede this ADR — do not silently fork navigation in code.

## Consequences

### Positive

- One place to change swipe semantics (e.g. invert carousel for left-handed desk placement).
- Features stay testable as draw + input handlers without duplicating CST816S code.
- Matches existing hardware (gestures) and AGENTS guidance to move touch out of `main.cpp` into `src/input/`.

### Negative / trade-offs

- CST816S gestures are coarse: no drag, limited simultaneous behaviour; some UIs may feel less “app-like” than a phone.
- Carousel order must be curated; many features mean more swipes unless we add ADR for a launcher (swipe up).
- Modal “capture” must be used sparingly or users lose consistent global navigation.

### Follow-ups

- ADR or feature spec for **swipe up** (settings vs app ring vs disabled).
- Persist **last active feature** across reboot (optional; clock-only default is fine for v1).
- Time source ADR when clock moves beyond `millis()` offset (NTP, RTC).
- Visual **transition** between features (slide vs cut) — cosmetic; shell should allow hook without changing nav policy.

## Alternatives considered

| Option | Why not (or deferred) |
|--------|------------------------|
| Each feature polls touch in its own module | Duplicated debounce; inconsistent global swipes; hard to guarantee “home” behaviour. |
| LVGL / full UI framework | RAM and complexity; explicitly out of scope in AGENTS unless a future ADR adopts it. |
| Tap-only navigation (no swipe carousel) | Fewer accidental mode changes, but poor fit for round puck “spin the dial” feel; swipes already proven on hardware. |
| Fixed buttons on bezel | No physical buttons on ESP32-2424S012C; touch only. |
| Single monolithic `display.cpp` | Does not scale as features grow; no clear lifecycle for clock vs others. |

## References

- [src/features/clock/docs/README.md](../src/features/clock/docs/README.md) — home feature
- [docs/features/README.md](../docs/features/README.md) — feature doc index
- [docs/overview.md](../docs/overview.md) — extension checklist
- [docs/hardware.md](../docs/hardware.md) — CST816S, round display
- [CST816S library gestures](https://github.com/fbiego/CST816S) — `SWIPE_*`, `SINGLE_CLICK`, etc.
- Current scaffold: `src/main.cpp` (touch poll), `include/config.h` (`TOUCH_GESTURE_DEBOUNCE_MS`)

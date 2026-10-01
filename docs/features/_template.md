# Feature: &lt;title&gt;

Copy this file to **`src/features/&lt;kebab-name&gt;/docs/README.md`**, then fill it in. Link the feature from [features/README.md](README.md).

| Field | Value |
|-------|--------|
| Status | idea |
| Depends on | — |
| Primary input | touch / time / network / none |

## Summary

One or two sentences: what this feature is and when the user sees it.

## Why it’s fun

What makes this worth having on a desk puck—not just utility, but delight, calm, or play.

## Behaviour

### Entry and exit

- How the user reaches this mode (boot default, swipe, long-press, schedule, etc.).
- How they leave it.

### On-screen

- Layout notes for the **240×240 round** safe area (~radius 100 px for primary content).
- Motion or refresh rules (static vs animated; update interval).

### Interaction

- Gestures, taps, or no touch (ambient only).

### Edge cases

- No WiFi, wrong timezone, sleep/wake, first boot, etc.

## Non-goals

What we are explicitly not doing in v1.

## Technical notes (optional)

- Time source, storage, RAM-heavy assets—only when it affects the spec.
- Suggested `src/` modules when known.

## Open questions

- [ ] …

# Features (firmware modules)

Each product feature lives in **one folder** under `src/features/<name>/`: runtime code, feature headers, assets, authoring tools, and design docs (except repo-wide [docs/](../../docs/) for hardware, bring-up, and ADRs).

## Layout (per feature)

```
src/features/<name>/
├── README.md           # optional one-screen index (clock has docs/README.md)
├── docs/               # feature design & authoring specs
├── assets/             # clock: faces/<slug>/ PNGs + source/ (not compiled)
├── tools/              # packers, one-off prep scripts (not compiled)
├── include/<name>/     # public headers (#include "<name>/…")
├── generated/          # build outputs (clock watch-face blobs only)
├── …                   # .cpp implementation
```

PlatformIO excludes `assets/`, `docs/`, and `tools/` from compilation via `build_src_filter` in [platformio.ini](../../platformio.ini).

## Features today

| Folder | Shell name | Notes |
|--------|------------|--------|
| [clock/](clock/) | Clock | Procedural + asset watch faces; [clock/docs/README.md](clock/docs/README.md) |
| [diary/](diary/) | Diary | Google Calendar agenda; OAuth branding in `diary/assets/branding/` |
| [patterns/](patterns/) | Patterns | Nav / demo screens |
| [cube/](cube/) | Cube | Tumbling shaded wireframe cube |
| [whatsapp/](whatsapp/) | WhatsApp | Latest message previews via LAN bridge; [whatsapp/docs/README.md](whatsapp/docs/README.md) |

## Shared infrastructure (not features)

| Path | Role |
|------|------|
| `src/app/` | Feature registry, navigation ([ADR-01](../../adr/ADR-01-touch-navigation-and-app-shell.md)) |
| `src/display/`, `src/input/` | LovyanGFX, CST816S |
| `src/wifi/`, `src/time/` | Network and wall clock used by diary / clock |
| `include/` | Cross-cutting headers (`config.h`, `feature.h`, …) |

## Adding a feature

1. Create `src/features/<slug>/` with `include/<slug>/`, register in `src/app/app_shell.cpp`.
2. Add `+<features/<slug>/>` and `-<features/<slug>/assets/>` etc. to `platformio.ini` if not covered by the existing pattern.
3. Add `-I src/features/<slug>/include` to `build_flags`.
4. Put specs in `src/features/<slug>/docs/` and link from [docs/features/README.md](../../docs/features/README.md).

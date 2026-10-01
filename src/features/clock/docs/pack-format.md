# Watch face pack format (v1)

Binary layout produced by [`tools/watch_face_pack.py`](../../../tools/watch_face_pack.py) and embedded in flash. The same byte sequence can be stored in LittleFS later as `<slug>.bin`.

## Face blob layout

All values **little-endian**.

| Offset | Size | Field |
|--------|------|--------|
| 0 | 4 | Magic `WPCK` |
| 4 | 2 | Version `1` |
| 6 | 2 | Flags: `0x0001` second, `0x0002` hub, `0x0004` chrono hand, `0x0008` chronograph behaviour |
| 8 | 2 | Pivot X (default 120) |
| 10 | 2 | Pivot Y (default 120) |
| 12 | 115200 | Dial RGB565, row-major 240×240 |
| … | variable | Chunks in order: hour, minute, [second], [chrono], [hub] |

### Hand or hub chunk

| Field | Size | Description |
|-------|------|-------------|
| width | 2 | uint16 |
| height | 2 | uint16 |
| pivot_x | 2 | int16, sprite pivot for rotation |
| pivot_y | 2 | int16 |
| reserved | 2 | uint16, zero |
| pixels | width×height×3 | RGB565 LE + alpha8 per pixel |

Runtime hand drawing uses [`AssetFaceMeta`](../../generated/watch_face_manifest.h) from the generated manifest (dimensions and `offset_deg` from build-time `face.json`).

## Authoring

See [authoring-spec.md](authoring-spec.md). Packer skips folders under `assets/faces/` whose names start with `_`.

## Regenerate

```powershell
python tools/watch_face_pack.py
```

PlatformIO runs this automatically via `extra_scripts` before each build.

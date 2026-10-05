# Clock feature assets

Shipped watch faces live under **`faces/<slug>/`**. Each slug is self-contained: firmware PNGs, optional authoring raws, and notes.

## Layout (per face)

```
faces/<slug>/
├── face.json          # manifest — required for pack
├── dial.png           # 240×240 RGB
├── hour.png, minute.png, …
├── hub.png            # optional
└── source/            # not flashed — JPG/PSD, prompts, README
    ├── README.md
    └── *_raw.jpg
```

The build packer reads only **`face.json` and the PNG paths it references** at the slug root. Everything under `source/` is ignored.

Folders whose names start with **`_`** (e.g. `_template`) are skipped.

## Pipeline

```text
faces/<slug>/source/*_raw.jpg  →  face-specific prep script  →  faces/<slug>/*.png
faces/<slug>/                  →  tools/watch_face_pack.py     →  generated/
```

| Face | Prep (optional) | Pack |
|------|-----------------|------|
| **steampunk** | `tools/faces/steampunk/generate.py` | always |
| **classic-chrono** | `tools/faces/classic-chrono/generate.py` | always |
| **wayfinder** | `tools/faces/wayfinder/generate.py` | always |
| **bell-ross** | `tools/faces/bell-ross/generate.py` (WatchMaker `.watch` in `source/`) | always |

**WatchMaker drop folder:** put `.watch` files in [`watch_files/`](../../../watch_files/) and run `python tools/watch_import.py`.

From repo root:

```powershell
python tools/watch_face_pack.py
# or: pio run  (runs packer via extra_script)
```

## Adding a face

1. Copy `faces/_template/` to `faces/<slug>/` (no `_` prefix).
2. Add PNGs + edit `face.json` ([authoring-spec.md](../docs/authoring-spec.md)).
3. **Live time/date (optional):** set `behaviour.digital_readout` — do not put ticking digits in `dial.png`. Layout/colours in JSON; see [digital-readout-compositing.md](../docs/digital-readout-compositing.md) and [wayfinder/face.json](faces/wayfinder/face.json).
4. Put generation output and notes in `faces/<slug>/source/` only.
5. Add `tools/faces/<slug>/generate.py` (or `prepare.py`) if you need asset prep.
6. Rebuild — packer updates `generated/`; no C++ registry edit.

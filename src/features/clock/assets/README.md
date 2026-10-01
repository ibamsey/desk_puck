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
| **demo** | (packer can generate placeholders) | always |
| **aurora** | `tools/faces/aurora/prepare.py` | always |
| **steampunk** | `tools/faces/steampunk/generate.py` | always |
| **classic-chrono** | `tools/faces/classic-chrono/generate.py` | always |

From repo root:

```powershell
python tools/watch_face_pack.py
# or: pio run  (runs packer via extra_script)
```

## Adding a face (e.g. steampunk)

1. Create `faces/steampunk/` with `face.json` and PNGs (see [docs/authoring-spec.md](../docs/authoring-spec.md)).
2. Put generation output and notes in `faces/steampunk/source/` only.
3. Add a prep script under `tools/faces/steampunk/` if you need JPG → PNG conversion.
4. Rebuild — no C++ registry edit.

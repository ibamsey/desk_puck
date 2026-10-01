# Aurora — authoring sources

Nothing here is flashed. Prep writes PNGs to the slug root (`../`).

| File | Used by prep |
|------|----------------|
| `dial_raw.jpg` | Yes → `dial.png` |
| `hour_raw.jpg` | Yes → `hour.png` |
| `hub_raw.jpg` | Yes → `hub.png` |
| `minute_raw.jpg`, `second_raw.jpg` | Archive only — minute/second are drawn in prep (JPG extract was unreliable) |

If `source/` has no JPGs, `prepare.py` writes a **procedural placeholder** dial/hub so the face still packs; add raws and re-run to restore AI art.

```powershell
python src/features/clock/tools/faces/aurora/prepare.py
python tools/watch_face_pack.py
```

Legacy filenames `aurora_*_raw.jpg` are still accepted.

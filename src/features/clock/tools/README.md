# Clock build tools

| Script | Scope |
|--------|--------|
| [`watch_face_pack.py`](watch_face_pack.py) | **All faces** — validates PNGs, writes `generated/` (also run on `pio run`) |
| [`faces/<slug>/…`](faces/) | **Optional per face** — regenerate art from sources; not required for firmware build |

Only the packer is generic. Face folders (`aurora/prepare.py`, `steampunk/generate.py`, …) exist when JPG→PNG or procedural authoring is non-trivial. If PNGs at `assets/faces/<slug>/` are already correct, skip them.

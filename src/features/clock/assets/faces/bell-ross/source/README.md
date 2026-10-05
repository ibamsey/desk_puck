# Bell & Ross BR S (WatchMaker source)

Source: `bell--ross-br-s-white-ceramic.watch` (WatchMaker ZIP). Regenerate PNGs:

```powershell
python src/features/clock/tools/faces/bell-ross/generate.py
python tools/watch_face_pack.py
```

Date `{dd}` and battery `{br}` from the original face are not ported.

The second hand uses WatchMaker placement `x=0`, `y=75` (cc) → `hands.second.anchor` in `face.json` (desk ~120,155).

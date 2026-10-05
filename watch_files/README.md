# WatchMaker imports

Drop **WatchMaker** `.watch` files here, then from the repo root:

```powershell
python tools/watch_import.py
```

Or import one file:

```powershell
python tools/watch_import.py watch_files\MyFace.watch --slug my-face --name "My Face"
```

Output: `src/features/clock/assets/faces/<slug>/` (`face.json`, `dial.png`, hands, `source/` copy of the `.watch`).

The importer:

- Composites a static **dial** from `watch.xml` (skips hand rotations and live text; date **window** artwork stays on the dial).
- Maps **date windows** (`{dd}` / `{d}` text layers) to `behaviour.digital_readout` (day-only, position/colour from XML).
- Detects **chrono subdials** from off-centre layers (`{drs}` wall seconds, `{drss}` chrono seconds, `{swm}` chrono minutes) and writes `subdials[]` with dial pivot + `hand_pivot` in `face.json`.
- Hand PNGs are cut **per WatchMaker layer** (same asset file, different x/y → separate sprites), rotated around the layer pivot; mains use face pivot `(120,120)`.
- Extracts **hour / minute / second** PNGs and maps **dial anchors** when a hand’s WatchMaker `x`/`y` (cc) is not the face centre — same rules as [bell-ross](../src/features/clock/assets/faces/bell-ross/).
- Runs `watch_face_pack.py` unless you pass `--no-pack`.

Large `.watch` files are usually gitignored; keep a copy elsewhere if needed.

**Protected exports** (`protection="y"`, `watch.pxml`, `.ppng` images) cannot be imported — layer PNGs are encrypted. Export again from WatchMaker with protection disabled, or use an unprotected `.watch` from the author.

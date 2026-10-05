# Wayfinder face — art notes

Tactical / outdoor watch aesthetic inspired by high-contrast “wayfinder” smartwatch layouts: black field, white block hour numerals, compass ring, safety-orange accents, radial grid texture.

## Production

Procedural PNGs from:

```powershell
python src/features/clock/tools/faces/wayfinder/generate.py
```

Dial is rendered at 480×480 with LANCZOS downscale to 240×240 for crisp edges. Analog hands are separate RGBA sprites; **digital time and date** are live in firmware (`behaviour.digital_readout` in `face.json`) using the static compositor — see [digital-readout-compositing.md](../../../docs/digital-readout-compositing.md).

Reference mood board: user-provided wayfinder-style mock (not flashed).

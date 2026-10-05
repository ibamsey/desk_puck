# New asset face template

Copy this folder to `faces/<your-slug>/` (drop the leading `_`), rename `face.json` fields, add PNGs at the slug root, then run the packer.

## Required reading

| Doc | Purpose |
|-----|---------|
| [authoring-spec.md](../../../docs/authoring-spec.md) | PNG rules, layer order, `face.json` |
| [digital-readout-compositing.md](../../../docs/digital-readout-compositing.md) | Live time/date (Wayfinder-style) |
| [ai-watch-face-agent.md](../../../docs/ai-watch-face-agent.md) | Full AI agent workflow |
| [assets/README.md](../../README.md) | Pipeline and pack |

## Live digital time/date (optional)

- Set `behaviour.digital_readout` to `true` or an object (see `_template/face.json` and [wayfinder/face.json](../wayfinder/face.json)).
- Leave the dial centre calm; **do not** bake digits into `dial.png`.
- Firmware compositing handles z-order (digits under hour/minute, second on top).

Put JPG/PSD/prompts under `source/` only — the packer ignores that subfolder.

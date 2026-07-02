# image/alpha_dualplate — fractional alpha from a plate pair (path 2)

Recovers FRACTIONAL alpha (glow, glass, smoke, soft cast shadow) from two aligned
generations of the same subject on different flat backgrounds (default white
`#fff` + black `#000`). Uses the Smith & Blinn (1996) Theorem-4 joint-channel
projection: with `D = bg_light - bg_dark` and `O = light - dark`,
`alpha = clip(1 - (O·D)/(D·D))`. Foreground = un-composite each plate and average.

Dual-plate is exact ONLY when the two plates show the same subject in the same
place, so a pair-consistency gate runs BEFORE extraction and fails a
redrawn/misaligned pair.

## Entry

- `dual_plate_alpha.py` -> `extract_dual_plate_alpha(...)` (CLI: `--light`,
  `--dark`, `--output`, `--alpha-combine proj`). Imports the pair gate.
- `dual_plate_pair_gate.py` -> `evaluate(light, dark)` -> verdict
  `pass` | `align` | `regenerate`.

Paired with `.codex/skills/nt-asset-image-generation/scripts/gen_dual_plate.sh`
(generates the white/black pair and calls the gate).

## Python deps

numpy, Pillow (studio `.venv`).

## Tests

`dual_plate_alpha_test.py`, `dual_plate_pair_gate_test.py`
(`.venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.alpha_dualplate.dual_plate_alpha_test`).

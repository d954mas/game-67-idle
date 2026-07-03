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
  `--dark`, `--output`, `--alpha-combine proj`). Imports the pair gate and the
  aligner; alignment runs before gating/extraction by default (`--skip-align`
  to disable).
- `dual_plate_pair_gate.py` -> `evaluate(light, dark)` -> verdict
  `pass` | `align` | `regenerate`. Also exposes `compute_inconsistency(light_rgb,
  dark_rgb)`, the core metric, so the aligner optimizes the SAME objective
  instead of inventing a second one.
- `pair_align.py` -> `align_pair(light, dark, max_shift=8)` (T0243): when the
  gate says "align" ("a translation align may rescue it"), searches small
  integer translations of the dark plate that minimize the gate's own
  inconsistency metric and returns `(dx, dy, fraction, aligned_dark)`. Never
  returns a shift worse than doing nothing. CLI: `--light`, `--dark`,
  `--max-shift`, `--output` (aligned dark plate), `--json-output`.

Paired with `.codex/skills/nt-asset-image-generation/scripts/gen_dual_plate.sh`
(generates the white/black pair and calls the gate).

## Python deps

numpy, Pillow (studio `.venv`).

## Tests

`dual_plate_alpha_test.py`, `dual_plate_pair_gate_test.py`, `pair_align_test.py`
(`.venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.alpha_dualplate.dual_plate_alpha_test`).

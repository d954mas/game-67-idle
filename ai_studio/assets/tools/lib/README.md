# Asset Tools Library

Small Python helpers shared by asset tools.

This group is not a public workflow surface. Use it for reusable, low-level
helpers that support `tools/` commands, such as atomic writes. Domain decisions
belong in the caller module.

## Modules

- `atomic_io.py` — atomic text/JSON/file/image writes (tmp file + `replace`).
- `color.py` (T0254) — the one shared color primitive: `key_distance` (Chebyshev
  color-distance-to-key, vectorized numpy), `parse_hex`/`format_hex` (`#rrggbb`),
  `estimate_border_key` (mode-of-opaque-border key auto-detect), `split_alpha`/
  `merge_alpha` (RGB-only processing that must leave alpha byte-identical).
  Consolidated from >=5 independent copies across alpha_matte/bg_fix/regions/
  route that had drifted (two different distance metrics, mode vs median border
  estimation) -- import from here instead of re-deriving any of these.

## Tests

`.venv/Scripts/python.exe -m unittest ai_studio.assets.tools.lib.atomic_io_test
ai_studio.assets.tools.lib.color_test`

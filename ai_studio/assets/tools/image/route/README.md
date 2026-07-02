# image/route — cutout path router

Decides key-matte vs dual-plate for one flat-key source crop, FROM THE SINGLE
FLAT-KEY SOURCE, so the choice can be made before a generation is spent.
Discriminator: the fraction of "partial" keyness mass (a blend of key +
foreground over a wide band) plus how deep that transition runs. Opaque art has a
bimodal keyness histogram (tiny partial ring); soft art has a fat middle / deep
gradient.

Route to dual-plate when `soft_score >= 0.11` OR `depth90 >= 14px` (the OR catches
a thin-but-deep glow ring). CPU + numpy/scipy only; no ML.

## Entry

- `route_cutout.py` -> `route_cutout(image, key=None) -> RouteDecision` and
  `soft_metrics(image, key=None) -> dict`. CLI `--auto-dual` launches
  `.codex/skills/nt-asset-image-generation/scripts/gen_dual_plate.sh` when soft.

## Python deps

numpy, scipy, Pillow (studio `.venv`).

## Tests

`route_cutout_test.py`
(`.venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.route.route_cutout_test`).

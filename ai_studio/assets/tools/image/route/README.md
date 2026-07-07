# image/route — cutout path router

Decides key-matte vs dual-plate for one flat-key source crop, FROM THE SINGLE
FLAT-KEY SOURCE, so the choice can be made before a generation is spent.
Discriminator: the fraction of "partial" keyness mass (a blend of key +
foreground over a wide band) plus how deep that transition runs. Opaque art has a
bimodal keyness histogram (tiny partial ring); soft art has a fat middle / deep
gradient.

Route to dual-plate when `soft_score >= 0.11` OR `depth90 >= 14px` (the OR catches
a thin-but-deep glow ring). CPU + numpy/scipy only; no ML.

## Что и когда (alpha routing, lead-ratified)

This module's own discriminator only decides **key_matte vs dual_plate** for a
flat-key source (UNCHANGED). The wider alpha-method portfolio the canvas exposes
routes as below (alpha bench 2026-07-07, lead-ratified). Pick the earliest row
whose condition matches:

| Case | Method | Cost / venv | Notes |
| --- | --- | --- | --- |
| Hard edge on a flat key | `key_matte` (`../alpha_matte/`) | ~0.3s CPU, shared venv | Crisp opaque sprites; exact color. Also the DEFAULT for FLAT magenta. |
| Glow / soft bloom on a key | **`corridorkey` FIRST** (canvas), `vitmatte` second | ~15s GPU / ~1-3s GPU | CorridorKey despills natively (lead's eye ruling on glow-wings); ViTMatte wins raw alpha but leaves more residual tint. |
| Thin detail (spider-web / mesh / fur / hair) on a key | `vitmatte` + despill (`../vitmatte_matte/`) | ~1-3s GPU, OWN venv | ~2x more accurate than CorridorKey on strand-level structure. Green/magenta key. |
| Arbitrary / unknown background, NO key | `birefnet` (`../birefnet_cutout/`) | ~10-30s CPU, shared venv | MIT; weak on flat line-art (SOD training distribution is photographic). |
| Translucent uniform interior (ghost / glass) | **dual-plate ONLY** (`../alpha_dualplate/`) | 2 generations at the GENERATION stage | Fractional alpha is unrecoverable from one plate — needs a white+black pair; this CHANGES the art (re-generation), not a post cut. |

Verdicts: alpha bench 2026-07-07, lead-ratified. Evidence: canvas project
`alpha-bench-2026-07-07-t0335-329849` + YandexDisk
`gamedev/ai_studio/alpha_bench_2026-07-07`. `corridorkey`/`vitmatte`/`birefnet`
are EXPLICIT-ONLY canvas alpha methods — this auto router never yields them; it
still only emits `key_matte` or `dual_plate`.

## Entry

- `route_cutout.py` -> `route_cutout(image, key=None) -> RouteDecision` and
  `soft_metrics(image, key=None) -> dict`. CLI `--auto-dual` launches
  `.codex/skills/nt-asset-image-generation/scripts/gen_dual_plate.sh` when soft.

## Python deps

numpy, scipy, Pillow (studio `.venv`).

## Tests

`route_cutout_test.py`
(`.venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.route.route_cutout_test`).

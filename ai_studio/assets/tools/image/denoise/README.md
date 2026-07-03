# image/denoise — light median denoise (RGB only)

Deliberately simple median-filter denoise for noisy AI art — no ML restorer, no
opencv/skimage (cut in the T0207 research pass,
`tmp/research_art_cleanup_2026-07-03.md`). One of the two Cleanup tools on the
canvas (the other is `../quantize/`); bg-solidify is NOT a standalone tool here —
it stays an internal pre-pass of the alpha keyer only.

## Entry

- `denoise.py` -> `denoise_image(image, strength) -> (Image, stats)` — `strength`
  is 1, 2, or 3, mapped to a median-filter pass ladder on RGB ONLY: 1 = one 3px
  pass, 2 = two 3px passes, 3 = one 5px pass. The alpha channel is split out
  BEFORE filtering and reattached byte-identical afterward — it is NEVER
  filtered (the halo law: a keyed/soft edge's alpha must stay exactly what the
  keyer produced).

CLI: `--source <png> --out <png> --strength 1|2|3 [--report <json>]`.

## Report schema

`ai_studio.image_tools.denoise_report.v1`: `{schema, source, strength, changed_pixel_pct}`
— `changed_pixel_pct` is the % of pixels whose RGB moved (0% on a flat image:
nothing to denoise).

## Python deps

numpy, Pillow (studio `.venv`).

## Tests

`denoise_test.py`
(`.venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.denoise.denoise_test`).

## Canvas integration

`ai_studio/assets/canvas/ops.mjs` (`cleanupPreview`/`cleanupApply`) spawns this
script through the shared image-tools warm worker; see that module's "cleanup"
section for the non-destructive preview/apply contract.

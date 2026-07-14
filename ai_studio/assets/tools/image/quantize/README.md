# image/quantize — palette quantization (color-count reduction)

Reduce an image's RGB color count (MEDIANCUT, alpha untouched) — the fix for
AI-generated color-banding/gradient-noise artifacts (T0207 research,
`ai_studio/assets/canvas/contracts/alpha-and-cleanup.md`). One of the two Cleanup tools on the
canvas (the other is `../denoise/`); bg-solidify is NOT a standalone tool here —
it stays an internal pre-pass of the alpha keyer only.

## Entry

- `quantize.py` -> `quantize_image(image, colors, *, dither=False) -> (Image, stats)`
  — `colors` is 2..256. The alpha channel is split out BEFORE quantizing (Pillow
  can only quantize RGBA with FASTOCTREE, which is not what this tool wants) and
  reattached byte-identical afterward, so alpha NEVER changes — only RGB does
  (including RGB under fully transparent pixels, which is invisible and fine).
  `dither=False` (default) is an exact nearest-palette-color mapping;
  `dither=True` applies Floyd-Steinberg.

CLI: `--source <png> --out <png> --colors N [--dither] [--report <json>]`.

## Report schema

`ai_studio.image_tools.quantize_report.v1`:
`{schema, source, colors_requested, dither, palette_size_before, palette_size_after, changed_pixel_pct}`.
`palette_size_before`/`_after` count unique RGB triples (capped at 100000 for a
pathological source); `changed_pixel_pct` is the % of pixels whose RGB moved.

## Python deps

numpy, Pillow (studio `.venv`).

## Tests

`quantize_test.py`
(`.venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.quantize.quantize_test`).

## Canvas integration

`ai_studio/assets/canvas/ops.mjs` (`cleanupPreview`/`cleanupApply`) spawns this
script through the shared image-tools warm worker; see that module's "cleanup"
section for the non-destructive preview/apply contract.

# image/slice — region slicing, per-region alpha, review + export

Apply detected or hand-edited regions to a source image and write separate image
files. This is the stage that turns reviewed regions into PNG slices, a manifest,
an optional review sheet, and an optional ZIP.

## Entry

- `slice_regions.py` -> `slice_regions(...)` (CLI `--source --regions
  --output-dir --prefix [--review-sheet] [--zip-output] [--manifest-output]
  [--key-color]`): read reviewed region JSON, crop each selected `rect`, apply
  the per-region alpha policy, optionally mask the crop with a region-level
  `polygon`, write PNG outputs + a manifest, optionally build `review_sheet.png`,
  and optionally write a ZIP archive.
- Bridge: `api.mjs` -> `reviewImageRegions`, `exportImageRegions`,
  `exportImageRegion` (thin wrappers over `_bridge`; the composed HTTP handler
  lives in `../api.mjs`).

Region `id` is the technical key; optional region `name` is the human export
label (used for slice filenames and review-sheet labels, de-duplicated with a
numeric suffix). Polygon points are source-image coordinates; the crop rect stays
the bounding box while pixels outside the polygon become transparent.

## Per-region alpha policy (folded in from the old raster2d/alpha)

Alpha is selected per reviewed region:

- `alpha.mode = "key_matte"` (default): deterministic flat-key cutout, applied
  automatically during review and export via `../alpha_matte/key_matte.py`. There
  is NO silent fallback -- if the key-matte tool is unavailable, slicing raises.
- `alpha.mode = "generation"`: mark the region for generated/dual-plate alpha
  instead. Export keeps the source crop and writes `alpha.status =
  "needs_generation"` in the manifest so the region can be regenerated
  deliberately (see `../alpha_dualplate/`).

## Review outputs (folded in from the old raster2d/review)

Human/agent review artifacts after detection + slicing: rect-overlay previews and
labelled contact sheets. The first review sheet is produced by
`slice_regions.py --review-sheet`.

## Python deps

Pillow, numpy, scipy (studio `.venv`; key-matte pulls scipy transitively).

## Tests

`slice_regions_test.py`, `api.test.mjs`
(`.venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.slice.slice_regions_test`;
`node --test ai_studio/assets/tools/image/slice/api.test.mjs`).

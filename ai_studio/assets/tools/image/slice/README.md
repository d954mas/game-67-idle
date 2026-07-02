# Raster 2D Slicing

Stage for applying detected or hand-edited regions to a source image and
writing separate image files.

Current tool:

- `slice_regions.py`: read reviewed region JSON, crop each selected `rect`,
  apply per-region alpha policy, optionally mask that crop with a region-level
  `polygon`, write PNG outputs, write a manifest, optionally build
  `review_sheet.png`, and optionally write a ZIP archive.

Region `id` is the technical key used by the review UI. Optional region `name`
is the human export label: when present, sliced PNG filenames and review-sheet
labels use `name`; duplicate names get a numeric suffix instead of overwriting
another slice.

Polygon points use source-image coordinates. The crop rect remains the bounding
box, so downstream tools can still reason about image size while pixels outside
the polygon become transparent.

By default, every region uses `alpha.mode = "key_matte"` and the flat key color
is cut out during slicing. Set `alpha.mode = "generation"` on a region to keep
the source crop and mark it as `needs_generation` in the manifest for a later
generated/dual-plate pass.

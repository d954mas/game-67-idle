# Raster 2D Review

Stage for human/agent review outputs after detection, slicing, and alpha
processing.

Expected outputs:

- rect overlay previews.
- contact sheets with labels. The first implementation is
  `../slicing/slice_regions.py --review-sheet`.
- JSON/Markdown reports describing counts, missing files, and visual risks.

Existing legacy review atlas tools remain in `../../review_atlas/` until moved
deliberately.

# Raster 2D Alpha

Alpha is selected per reviewed region.

Default region policy:

- `alpha.mode = "key_matte"`: deterministic flat-key cutout. This is the normal
  path and is applied automatically by `../slicing/slice_regions.py` during
  review and export.

Manual override:

- `alpha.mode = "generation"`: mark this region for generated/dual-plate alpha
  instead of applying the default key matte. Export keeps the source crop and
  writes `alpha.status = "needs_generation"` in the manifest so the region can
  be regenerated deliberately.

The browser Asset Tools inspector can preview both policies. `key_matte` shows
the deterministic matte result. `generation` shows a diagnostic matte view with
edge pixels marked for the later generated/dual-plate pass; it is not the final
generated alpha output.

Other alpha operations should stay explicit:

- flatten alpha: RGBA to opaque RGB/RGBA on a chosen background.
- threshold alpha: soft alpha to binary alpha.
- transparent RGB hygiene: remove hidden color fringe under transparent pixels.

Existing legacy cutout tools remain in `../../cutout/` until moved deliberately.

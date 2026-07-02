# raster2d — temporary canvas seam (shim)

This folder is a temporary seam, not a toolbox. The 2D image tools were
decomposed into per-tool folders under `../image/`
(`sources/`, `bg_fix/`, `regions/`, `slice/`, `alpha_matte/`, `alpha_dualplate/`,
`route/`, over the shared `_bridge/`). The studio shell now wires the composed
handler from `../image/api.mjs`.

`api.mjs` here only re-exports the old `raster2d` names
(`uploadRaster2dSource` -> `image/sources`, `detectRaster2dRegions` ->
`image/regions`, plus the composed handler) because the canvas still imports two
of them (`ai_studio/assets/canvas/ops.mjs`). The canvas migrates to
`../image/{sources,regions}/api.mjs` in increment 6, after which this folder is
deleted.

The public HTTP endpoint URLs `/api/asset-tools/raster2d/*` and the
`tmp/ai_studio/assets/raster2d/` path prefix are a stable contract for the frozen
Asset Tools viewer and stay unchanged — only the internal module layout moved.

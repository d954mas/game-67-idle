# Raster 2D Image Tools

Workflow tools and API bridge for generated or sourced 2D image
assets: source sheets, sprites, UI pieces, icons, decals, and flat raster props.

This module is the new Python-first structure for image work. Agents and the
Asset Tools surface in `ai_studio/assets/viewer/` use these same pipeline tools.
Runtime artifacts, uploads, intermediate files, review sheets, and ZIP exports
are written under `tmp/`.
Older 2D tools still live in legacy sibling folders until each step is moved
intentionally.

## Stages

- `sources/`: working source-image conventions, especially generated sheets kept
  under `tmp/` while they are being inspected.
- `background/`: normalize border-connected generated sheet backgrounds to an
  exact key color without touching interior key-colored art. Whole-image/no-
  background inputs can explicitly skip this step.
- `regions/`: detect isolated visual regions on a source sheet and emit crop
  rects. The site also lets the user add/delete rect regions and add polygon
  mask regions before export.
- `slicing/`: apply reviewed rect or polygon regions and their alpha policy to
  produce separate PNG files, review sheets, manifests, ZIP exports, and direct
  single-region PNG exports from the site.
- `alpha/`: per-region alpha policy. Default is deterministic `key_matte`;
  selected regions can be marked for generated/dual-plate alpha.
- `review/`: visual review conventions. The first review sheet is currently
  produced by `slicing/slice_regions.py`.

## Site Feature Shape

The browser workflow should mirror the same stages:

1. Upload/load source image; site stores it under `tmp/ai_studio/assets/raster2d/`.
2. Choose background mode and run detection. `Auto sheet background` runs Python
   `background -> regions`; `Whole image / no background` skips normalization
   and creates one full-image region.
3. Add/delete rects or polygon-mask regions in the site, name regions for
   readable export files, choose per-region alpha mode when needed, and persist
   reviewed regions to `tmp`. Region selection can zoom the canvas to that
   region and show source/alpha crop previews before export.
4. Use the site's separate Slice 9 mode to test fixed-edge/stretch-center UI
   images against either the whole source image or the selected region.
5. Run Python slicing/export to create PNG slices and optional review sheet ZIP.
   One selected region can also be saved directly as a PNG through the same
   slicing path.
6. Later stages apply alpha cleanup and promote accepted assets to durable game
   storage with provenance.

This module does not own license decisions, provenance records, manifests,
storage indexes, or game-engine loading.

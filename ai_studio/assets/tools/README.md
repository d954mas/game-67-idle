# Asset Tools

Concrete utilities for working on asset source files before they enter storage,
template assets, or a game project.

Asset tools do not own manifest metadata, licenses, indexes, or storage writes.
They produce normalized files, region JSON, sliced images, review sheets, ZIP
exports, audit JSON, Markdown reports, preview images, and local browser exports
that can be referenced by intake, game-owned generation notes, or quality
evidence. Working outputs belong under `tmp/`; accepted assets are promoted
later by the owning game/template/shared storage flow.

Review and tests are step-local verification. They run after a concrete action,
such as slicing a source sheet, removing alpha, extracting a cutout, converting a
model, or checking a texture. They are not a separate top-level asset group.

## Groups

- `lib/`: low-level shared Python helpers for asset tools.
- `raster2d/`: Python-first staged structure for generated or sourced 2D image
  work. It owns reusable pipeline tools and the API bridge used by the browser
  Asset Tools surface in `../viewer/`: source images in `tmp`, background
  normalization, region detection, manual region review, slicing, alpha cleanup,
  review sheets, ZIP export, single selected-region PNG export, Slice 9 site
  testing, and later promotion handoff.
- `conversion/`: format conversion helpers such as OBJ/MTL to GLB.
- `source_sheets/`: generated UI/icon/sprite source sheet normalization and
  intake audits before cropping.
- `crop/`: source-sheet split planning and, later, manual crop-boundary editing.
- `cutout/`: alpha extraction, key-matte cutout, dual-plate extraction, and
  transparent-edge cleanup.
- `review_atlas/`: visual contact-sheet verification helpers used after
  prepared image outputs exist. This is not engine integration; the engine owns
  its own asset loading/build pipeline.
- `textures/`: standalone material texture checks, including 2x2 repeat preview
  generation and seam metrics.

The older `source_sheets/`, `crop/`, `cutout/`, and `review_atlas/` folders
remain in place until each step is moved into `raster2d/` with tests, site
wiring, and callers updated.

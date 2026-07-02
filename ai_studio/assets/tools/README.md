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
- `image/`: the 2D image pipeline, decomposed per media type over a shared
  `_bridge` (source intake, background normalization, region detection, slicing,
  key-matte and dual-plate alpha, routing). Used by agents and the `../canvas/`
  module: source images in `tmp`, region JSON, sliced images, review sheets, and
  ZIP/PNG exports.
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
remain in place until each step is moved into `image/` with tests and callers
updated.

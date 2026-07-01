# Asset Tools

Concrete utilities for working on asset source files before they enter storage,
template assets, or a game project.

Asset tools do not own manifest metadata, licenses, indexes, or storage writes.
They produce normalized files, audit JSON, Markdown reports, preview images, and
local browser exports that can be referenced by intake, game-owned generation
notes, or quality evidence.

Review and tests are step-local verification. They run after a concrete action,
such as slicing a source sheet, removing alpha, extracting a cutout, converting a
model, or checking a texture. They are not a separate top-level asset group.

## Groups

- `lib/`: low-level shared Python helpers for asset tools.
- `conversion/`: format conversion helpers such as OBJ/MTL to GLB.
- `manual/`: browser surface for local upload, alpha transforms, source slicing,
  crop-plan JSON export, and PNG/ZIP downloads.
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

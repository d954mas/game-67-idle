# Asset Prep

Reviewed tools for preparing asset source files before they enter storage,
template assets, or a game project.

Prep tools do not own manifest metadata, licenses, indexes, or browser UI. They
produce normalized files, audit JSON, Markdown reports, and preview images that
can be referenced by intake, art jobs, or quality evidence.

## Groups

- `lib/`: low-level shared Python helpers for prep tools.
- `conversion/`: format conversion helpers such as OBJ/MTL to GLB.
- `source_sheets/`: generated UI/icon/sprite source sheet normalization and
  intake audits before cropping.
- `crop/`: source-sheet split planning and, later, manual crop-boundary editing.
- `cutout/`: alpha extraction, key-matte cutout, dual-plate extraction, and
  transparent-edge cleanup.
- `review_atlas/`: visual contact-sheet and label review helpers. This is not
  engine integration; the engine owns its own asset loading/build pipeline.
- `textures/`: standalone material texture checks, including 2x2 repeat preview
  generation and seam metrics.

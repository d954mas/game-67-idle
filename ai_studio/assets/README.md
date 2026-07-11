# Assets

Reviewed AI Studio home for asset-related modules.

Top-level groups:

- `gallery/`: browser-facing asset surface. This is the clear visual group for
  browsing registered asset sources, inspecting packs/assets, reviewing
  game-local assets, promoting keepers, pulling reusable assets into a game,
  and recording gallery sessions.
- `tools/`: concrete asset work utilities. The 2D image pipeline is decomposed
  per media type under `tools/image/` (`sources`, `bg_fix`, `regions`, `slice`,
  `alpha_matte`, `alpha_dualplate`, `route`, over a shared `_bridge`), used by
  agents and the canvas module. Older source-sheet, crop, and review tools stay
  in place until replaced stage by stage.
- `canvas/`: multi-image canvas projects (Figma/Recraft-like). Every capability
  is one operation in a shared ops layer with two equal clients, the agent
  (CLI/import) and the thin browser page; `detect_regions` bridges to the image
  tools (`tools/image/{regions,sources}`) unmodified. See `canvas/README.md`.
- `backlog/`: temporary holding area for asset modules that still need a better
  decomposition. Storage, source registry, manifests, index, preview cache, and
  license guard live there until they are split into clearer groups.

Raster generation is handled by the `nt-asset-image-generation` skill after
source-first search fails. This module does not own shared generation job
scaffolds, prompt contracts, or prompt records. If a game needs durable
generation context, keep it with that game's design/assets metadata.

Asset sources are explicit. Global libraries are registered in
`backlog/storage/sources/libraries.json`; templates and games resolve through
the workspace catalog plus their identity manifests.
Asset Viewer does not scan the repository to guess source folders.

Asset refresh is explicit. Do not add filesystem watch mode to this module.
Local additions or edits should become visible through page reload, a manual
refresh/reindex action, or a targeted asset operation.

Large source reads go through `backlog/storage/index/`. Each source gets its own
generated SQLite database. The index is rebuilt explicitly from manifests, the
folder scan, then serves pages, search, filters, packs, and model lookups
without rescanning on every request.

Preview generation is local asset storage preparation, not browser rendering.
Asset Viewer should display prepared previews, report missing/stale previews,
and offer explicit refresh or regenerate actions that run local preparation jobs
outside the page.

This repository is public. Paid, private, unknown-license, or
non-redistributable asset binaries must stay out of git. The current canonical
guard lives in `backlog/storage/license/` while storage remains in backlog.

# Assets

Reviewed AI Studio home for asset-related modules.

Top-level groups:

- `viewer/`: browser-facing asset surface. This is the clear visual group for
  browsing registered asset sources, inspecting packs/assets, reviewing
  game-local assets, promoting keepers, pulling reusable assets into a game,
  and recording gallery sessions.
- `tools/`: concrete asset work utilities: source-sheet checks, crop planning,
  alpha/cutout cleanup, conversion, texture checks, and manual local prep UI.
- `backlog/`: temporary holding area for asset modules that still need a better
  decomposition. Storage, source registry, manifests, index, preview cache, and
  license guard live there until they are split into clearer groups.

Raster generation is handled by the `nt-asset-image-generation` skill after
source-first search fails. This module does not own shared generation job
scaffolds, prompt contracts, or prompt records. If a game needs durable
generation context, keep it with that game's design/assets metadata.

Asset sources are explicit. Global libraries are registered in
`backlog/storage/sources/libraries.json`, templates in
`templates/templates.json`, and games in `games/games.json`.
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

# Assets

Reviewed AI Studio home for asset-related modules.

Current migrated modules:

- `asset_viewer/`: browser surface and tools for browsing the shared asset
  library, reviewing game-local assets, promoting keepers, pulling reusable
  assets into a game, and recording gallery sessions.
- `assets_storage/`: storage layer for explicit source registries, OKF/Markdown
  catalog reading, local asset indexing, and derived preview preparation.

Asset sources are explicit. Templates are registered in
`assets_storage/source_registry/templates.json`; games are registered in
`assets_storage/source_registry/games.json`. Asset Viewer does not scan the
repository to guess template or game folders.

Asset refresh is explicit. Do not add filesystem watch mode to this module.
Local additions or edits should become visible through page reload, a manual
refresh/reindex action, or a targeted asset operation.

Large source reads go through `assets_storage/asset_index/`. Each source gets
its own generated SQLite database. The index is rebuilt explicitly from the
catalog or folder scan and then serves pages, search, filters, packs, and model
lookups without rescanning on every request.

Preview generation is local asset storage preparation, not browser rendering.
Asset Viewer should display prepared previews, report missing/stale previews,
and offer explicit refresh or regenerate actions that run local preparation jobs
outside the page.

Not yet migrated: the broader intake, conversion, audit, packing, and
product-gate asset pipeline under `tools/assets/`. Move those only after a
separate ownership review.

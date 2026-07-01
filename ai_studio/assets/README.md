# Assets

Reviewed AI Studio home for asset-related modules.

Current migrated modules:

- `viewer/`: browser surface for browsing registered asset sources, inspecting
  packs/assets, reviewing game-local assets, promoting keepers, pulling
  reusable assets into a game, and recording gallery sessions.
- `prep/`: source-sheet, crop, cutout/alpha, and texture preparation before
  assets enter storage, templates, or game projects.
- `storage/`: source registration, canonical manifests, generated search index,
  snapshots, preview cache, and license guard.
- `workflow/`: agent-facing generated-art workflows, prompt packets, generation
  records, and provenance checks.

Asset sources are explicit. Global libraries are registered in
`storage/sources/libraries.json`, templates in `templates/templates.json`, and
games in `games/games.json`.
Asset Viewer does not scan the repository to guess source folders.

Asset refresh is explicit. Do not add filesystem watch mode to this module.
Local additions or edits should become visible through page reload, a manual
refresh/reindex action, or a targeted asset operation.

Large source reads go through `storage/index/`. Each source gets its own
generated SQLite database. The index is rebuilt explicitly from manifests, the
folder scan, then serves pages, search, filters, packs, and model lookups
without rescanning on every request.

Preview generation is local asset storage preparation, not browser rendering.
Asset Viewer should display prepared previews, report missing/stale previews,
and offer explicit refresh or regenerate actions that run local preparation jobs
outside the page.

This repository is public. Paid, private, unknown-license, or
non-redistributable asset binaries must stay out of git. Asset Storage owns the
canonical guard in `storage/license/`.

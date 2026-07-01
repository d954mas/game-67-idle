# Source Registry

Explicit registry of asset source roots used by AI Studio surfaces.

## Role

Source Registry owns the helper modules for explicit asset source roots that
should appear in Asset Viewer. It avoids guessing libraries, templates, or game
folders by scanning the repository root.

## Files

- `libraries.json`: registered global reusable asset library roots.
- `libraries.mjs`: small helper for listing and upserting library
  asset sources.
- `templates.mjs`: small helper for listing and upserting template
  asset sources. Template source data lives in `templates/templates.json`.
- `games.mjs`: small helper for listing and upserting game asset
  sources. Game source data lives in `games/games.json`.

`templates/templates.json` registers game template folders and their asset
roots. `ai_studio/bootstrap/new_game.mjs --template <id>` resolves template ids
from this file.

`games/games.json` registers game folders and their asset roots. It is not a
template lineage/provenance file; after copy, each game owns its folder.

New games created through `ai_studio/bootstrap/new_game.mjs` are registered in
`games/games.json`.

## Path Boundary

Library sources may point at external shared storage. Template and game sources
are repository-owned roots, so their `folder` and `assets` paths must be
repo-relative and cannot use absolute paths or `..` traversal.

# Source Registry

Explicit registry of asset source roots used by AI Studio surfaces.

## Role

Source Registry stores explicit asset source roots that should appear in Asset
Viewer. It avoids guessing libraries, templates, or game folders by scanning the
repository root.

## Files

- `libraries.json`: registered global reusable asset library roots.
- `libraries.mjs`: small helper for listing and upserting library
  asset sources.
- `templates.json`: registered template asset roots.
- `templates.mjs`: small helper for listing and upserting template
  asset sources.
- `games.json`: registered game asset roots.
- `games.mjs`: small helper for listing and upserting game asset
  sources.

New games created through `ai_studio/bootstrap/new_game.mjs` are registered here.

## Path Boundary

Library sources may point at external shared storage. Template and game sources
are repository-owned roots, so their `folder` and `assets` paths must be
repo-relative and cannot use absolute paths or `..` traversal.

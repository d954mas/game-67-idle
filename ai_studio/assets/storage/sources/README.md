# Source Registry

Explicit registry of asset source roots used by AI Studio surfaces.

## Role

Source Registry stores explicit asset source roots that should appear in Asset
Viewer. It avoids guessing templates or game folders by scanning the repository
root.

## Files

- `templates.json`: registered template asset roots.
- `templates.mjs`: small helper for listing and upserting template
  asset sources.
- `games.json`: registered game asset roots.
- `games.mjs`: small helper for listing and upserting game asset
  sources.

New games created through `tools/bootstrap/new_game.mjs` are registered here.

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
  asset sources backed by the workspace catalog and template manifests.
- `games.mjs`: small helper for listing public game asset sources backed by the
  workspace catalog and game manifests. Game creation writes through Workspace.

`ai_studio/workspace/catalog.json` mounts game template folders and their asset
roots. `games/new_game.mjs --template <id>` resolves template ids
from this file.

The same catalog mounts public/tracked game folders and their asset roots.
It is not a template lineage/provenance file; after copy, each game owns its
folder.

New public games created through
`games/new_game.mjs --visibility public` are registered in the tracked workspace catalog.
Private games created through `games/new_game.mjs --visibility private` must not
be added here.

Private commercial game mounts live in the ignored
`ai_studio/workspace/catalog.local.json` overlay and are resolved through
`ai_studio/workspace/games.mjs` only after explicit private opt-in and preflight.
Do not add private game entries to this public source registry.

## Path Boundary

Library sources may point at external shared storage. Template and game sources
are repository-owned roots, so their `folder` and `assets` paths must be
repo-relative and cannot use absolute paths or `..` traversal.

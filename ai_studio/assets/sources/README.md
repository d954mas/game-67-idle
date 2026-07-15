# Source Discovery

Asset source discovery used by AI Studio surfaces.

## Role

Source discovery owns helpers for roots that should appear in Asset Viewer.
Libraries remain explicit; templates and games are discovered from identity
manifests in their conventional folders.

## Files

- `libraries.json`: registered global reusable asset library roots.
- `libraries.mjs`: small helper for listing and upserting library
  asset sources.
- `templates.mjs`: helper for listing template asset sources from manifests.
- `games.mjs`: helper for listing public game asset sources from manifests.

`games/new_game.mjs --template <id>` resolves template ids by scanning
`templates/<id>/template.json`. Public games are discovered from
`games/<id>/game.json`; after copy, each game owns its folder.

Private commercial games live under ignored `games/private/<id>` and are
resolved only after explicit private opt-in and preflight.

## Path Boundary

Library sources may point at external shared storage. Template and game sources
are repository-owned roots, so their `folder` and `assets` paths must be
repo-relative and cannot use absolute paths or `..` traversal.

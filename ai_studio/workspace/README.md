# Workspace Discovery

Workspace owns the single resolver for game and template mounts. It scans
`templates/<id>`, `games/<id>`, and (when explicitly requested)
`games/private/<id>`; there is no mount registry.

- `games/<id>/game.json` and `templates/<id>/template.json` are the identity
  sources. Private games use `games/private/<id>/game.json`.
- `games/<id>/dependencies.json` records the exact engine and shared-feature
  revisions tested with that game checkout.
- `templates/<id>/game-dependencies.json` is the strict creation seed;
  `games/new_game.mjs` resolves its entries to clean, exact Git revisions.

Public mount roots must be direct children of `games/` or `templates/`; private
mounts must be direct children of `games/private/`. The resolver rejects public
symlinks/junctions and case-insensitive collisions in ids, namespaces, store ids,
or aliases. Incomplete folders without an identity manifest are skipped with a
warning.

Private entries are excluded by default. Tools must select an active private
game or explicitly request private mounts. The committed `games/private/`
ignore protects the parent repository; every private game keeps nested Git
metadata. Creating a private game installs a parent-repository pre-commit hook
that runs the privacy preflight. The preflight validates nested Git and scans
tracked parent text for private tokens.

```powershell
node ai_studio/workspace/games.mjs list --json
node ai_studio/workspace/games.mjs list --include-private --json
node ai_studio/workspace/games.mjs preflight --json
node --test ai_studio/workspace/tests/catalog.test.mjs
node --test ai_studio/workspace/tests/private_games_registry.test.mjs
```

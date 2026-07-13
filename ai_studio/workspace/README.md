# Workspace Catalog

Workspace owns the single resolver for game and template mounts.

- `catalog.json` is tracked and contains public mount facts.
- `catalog.local.json` uses the same `ai_studio.workspace.catalog.v1` schema,
  is ignored, and contains private or machine-local mount facts.
- `games/<id>/game.json` and `templates/<id>/template.json` are the only
  identity sources. Catalog rows must not duplicate ids, titles, or storage
  namespaces.
- `games/<id>/dependencies.json` records the exact engine and shared-feature
  revisions tested with that game checkout.
- `templates/<id>/game-dependencies.json` is the strict creation seed;
  `games/new_game.mjs` resolves its entries to clean, exact Git revisions.

Mount roots must be direct children of `games/` or `templates/`. The resolver
rejects unknown schemas and fields, missing manifests, roots outside the repo,
and case-insensitive collisions in roots, ids, namespaces, store ids, or safe
aliases. There is no legacy registry fallback.

Private entries are excluded by default. Tools must select an active private
game or explicitly request private mounts. The privacy preflight and Git guard
remain mandatory before a private mount feeds aggregate or parent-repository
operations. Ignore only `ai_studio/workspace/catalog.local.json` publicly; each
private `games/<id>/` root belongs in the parent checkout's `.git/info/exclude`.

```powershell
node ai_studio/workspace/games.mjs list --json
node ai_studio/workspace/games.mjs list --include-private --json
node ai_studio/workspace/games.mjs preflight --json
node ai_studio/workspace/games.mjs git-guard --command "git add ." --json
node --test ai_studio/workspace/tests/catalog.test.mjs
node --test ai_studio/workspace/tests/private_games_registry.test.mjs
```

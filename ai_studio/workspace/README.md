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

## Parallel feature workspaces

Use `feature_workspaces.mjs` when separate agents need to change different
features of the same private game. It creates one sibling workspace containing
a detached Studio worktree, the exact local engine gitlink, and a nested game
worktree on its own `agent/t####-name` branch. Manual copies of the Studio or
game are not part of the supported workflow.

Run commands from the Studio root. `new` accepts a dirty source checkout, but
uses only the committed Studio and game `HEAD` objects; staged, unstaged,
untracked, and ignored source files are never copied. The assigned task must be
an implementation-eligible committed card in the selected game's Taskboard.

When the source Studio has a configured root `.venv`, `new` derives its base
Python executable from `pyvenv.cfg`, creates a fresh root `.venv` inside the
Studio worktree, installs the pinned Studio requirements, and validates it with
`python_check.mjs`. It never copies or links the source environment. `check`
reports a missing, externally resolving, or otherwise invalid workspace Python
environment. When the source Studio has no configured `.venv`, Python setup is
reported as `not-configured` and workspace creation continues.

```powershell
node ai_studio/workspace/feature_workspaces.mjs new --game <game-id> --task T0001 --name <feature>
node ai_studio/workspace/feature_workspaces.mjs list
node ai_studio/workspace/feature_workspaces.mjs check <feature>
node ai_studio/workspace/feature_workspaces.mjs prepare-python <feature>
node ai_studio/workspace/feature_workspaces.mjs reallocate-ports <feature>
node ai_studio/workspace/feature_workspaces.mjs recover <feature>
node ai_studio/workspace/feature_workspaces.mjs remove <feature>
```

The default base is the sibling `<studio-folder>-workspaces`; pass `--base` to
every command when using another location. Each active workspace owns a
DevAPI/web port pair and records its immutable source commits in
`workspace.json`. Launch programs with those recorded ports; if another local
process races the advisory lease, stop workspace processes and run
`reallocate-ports`.

`remove` refuses staged, unstaged, or untracked changes in Studio, game, or
engine. It retains the game branch, even when unmerged. The registry moves a
completed removal to a tombstone so `check` can distinguish a removed workspace
from an unknown name. If creation or removal is interrupted, use `recover`; do
not delete or prune the worktrees by hand.

All commands accept `--json`. The implementation suite uses disposable local
repositories and never needs network access:

```powershell
node --test ai_studio/workspace/tests/feature_workspaces.test.mjs
```

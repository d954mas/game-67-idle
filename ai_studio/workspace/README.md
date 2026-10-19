# Workspace Discovery

The [reuse audit](reuse_audit.md) records the source decomposition, reusable
boundaries, and separation of the blank starter from the independent feature
consumer. The [iteration audit](iteration_audit.md) records recurring workflow
constraints, inspected evidence and measured Studio improvements.
The [iteration performance research](iteration_performance_research.md)
separates current native-loop measurements, dependency readiness, confirmed
overhead and withdrawn optimization proposals.

Workspace owns the single resolver for game and template mounts. It scans
`templates/<id>`, `games/<id>`, and (when explicitly requested)
`games/private/<id>`; there is no mount registry.

- `games/<id>/game.json` and `templates/<id>/template.json` are the identity
  sources. Private games use `games/private/<id>/game.json`.
- `games/<id>/dependencies.json` records the exact engine and shared-feature
  revisions tested with that game checkout. Games compile both from the Studio
  checkout they sit in, so a record is only true while it matches what is
  checked out; `version_drift.mjs` lists every record that does not (frozen
  games are skipped unless named).
- `game.json` may carry `lifecycle` for a frozen game; see
  `games/README.md#frozen-games`.
- Engine `version` can be the eight-character prefix of its pinned Git revision.
  Older engine pins may retain exact SemVer; feature versions remain SemVer.
- `templates/<id>/game-dependencies.json` is the strict creation seed;
  `games/new_game.mjs` resolves its entries to clean, exact Git revisions. The
  seed names no engine version: the engine has none besides its commit
  (`nt_version_engine()`), so a new game records the revision prefix.
  A game whose copied `tools/package_web.mjs` still reads `NT_VERSION_*` from
  `nt_core.h` fails on that engine until it takes the template's copy.

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
tracked parent text for private tokens. A private Canvas projects junction may
resolve outside its game only when its canonical game id and physical projects
root are registered in ignored `ai_studio/studio.config.local.json`; the hook
scans that external root directly and rejects unregistered or nested reparse
escapes.

```powershell
node ai_studio/workspace/games.mjs list --json
node ai_studio/workspace/games.mjs list --include-private --json
node ai_studio/workspace/games.mjs preflight --json
node ai_studio/workspace/games.mjs preflight --json --timings
node ai_studio/workspace/version_drift.mjs --include-private
node --test ai_studio/workspace/tests/catalog.test.mjs
node --test ai_studio/workspace/tests/private_games_registry.test.mjs
```

The commit preflight is separate from the game build/test/run loop. Text-only
changes do not walk private binary trees. A changed public binary compares
only size-compatible bytes: index lengths come from validated Git metadata;
worktree lengths and contents are checked through stable file handles. All
path, reparse and privacy guards remain active. One physical walk includes
ignored and untracked files; nested repositories and gitlinks retain their
separate ownership boundary. Exact sharing approvals are batched per Git root.
There is no persistent freshness cache. Use `--timings` to locate a slow stage.

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
node ai_studio/workspace/feature_workspaces.mjs new --game <game-id> --no-task --name <probe>
node ai_studio/workspace/feature_workspaces.mjs list
node ai_studio/workspace/feature_workspaces.mjs check <feature>
node ai_studio/workspace/feature_workspaces.mjs prepare-python <feature>
node ai_studio/workspace/feature_workspaces.mjs reallocate-ports <feature>
node ai_studio/workspace/feature_workspaces.mjs recover <feature>
node ai_studio/workspace/feature_workspaces.mjs remove <feature>
node ai_studio/workspace/feature_workspaces.mjs remove-merged [--apply]
node ai_studio/workspace/feature_workspaces.mjs forget-missing [--apply]
```

`--no-task` creates an exploration workspace: the same isolation, a game branch
named `agent/<name>` with no task id, and no Taskboard check. Use it for
experiments and probes that may be thrown away. Work that will merge into the
integration branch still needs a card: create the card first, or open a task
workspace and cherry-pick the probe onto it.

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
not delete or prune the worktrees by hand. Recovery of an interrupted removal
also finishes a worktree that Git had begun deleting (its `.git` link gone,
files left behind).

A worktree may have moved after creation. A Studio `HEAD` that a source branch
or remote-tracking ref still contains is accepted, as is a detached game
worktree at such a commit; a commit that exists only in the workspace refuses
removal. Ignored data is deleted only when it is generated:
top-level `build`, `tmp`, `out`, `.cache`, `.vite`, `.venv`, `cmake-build-*`,
and at any depth `__pycache__`, `build`, `src/generated`, `release/artifacts`.
Every ignored file under any folder named `build` therefore counts as generated
and is deleted; keep nothing irreplaceable in an ignored `build/`. Deletion
refuses any path that resolves outside its worktree through a link or junction.
An engine `HEAD`, branch, tag, or stash commit must already exist in the source
engine repository, because the workspace engine clone lives in the Studio
worktree's administration directory and is deleted with it.
Files under `.ai_studio/evidence/` and `assets/restricted/` refuse removal
unless the source checkout holds a byte-identical copy at the same path. Any
other ignored file refuses removal before the workspace changes state. Git runs
with `core.longpaths=true`, so paths over the Windows 260-character limit work.

`remove-merged` lists ready workspaces whose game branch is merged into the
integration branch recorded at creation (an ancestor, or every commit
patch-equivalent by `git cherry`) and that pass the same removal checks. It is a
dry run; `--apply` removes them one by one through `remove`. `forget-missing`
lists active registry entries whose workspace folder no longer exists and the
Git administration records of those vanished worktrees; `--apply` deletes only
those records and tombstones the entries. It never runs a repository-wide
`git worktree prune`, and it keeps an entry whose Studio record still holds
engine commits missing from the source engine. Neither command deletes a
branch. Recovery of an interrupted removal refuses a half-deleted worktree that
holds files written after the removal started.

All commands accept `--json`. The implementation suite uses disposable local
repositories and never needs network access:

```powershell
node --test ai_studio/workspace/tests/feature_workspaces.test.mjs
```

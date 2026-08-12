# Feature Workspaces Plan Review

## Verdict

PASS. Two independent reviewers converged with no unresolved HIGH or MEDIUM
findings.

Review lenses:

- implementation-plan completeness and repository contract grounding;
- Windows Git worktree/submodule safety, concurrent creation, crash recovery,
  Taskboard identity, and destructive path confinement.

The GSD phase-convergence runner was not applicable because this repository has
no `.planning/ROADMAP.md` or configured GSD phase. The same bounded
review-revise loop was run directly against the canonical workspace-module plan.

## Revision history

### Cycle 1 — REVISE

Resolved findings:

- Added one explicit base contract across `new`, `list`, `check`, `recover`,
  `reallocate-ports`, and `remove`.
- Added registry-first crash recovery and immutable ownership evidence.
- Added safe cleanup of ignored build output before non-force removal.
- Added deterministic dual-workspace native and web smoke verification.
- Replaced hard-coded `master` comparison with the recorded integration ref.
- Defined registry tombstones and post-removal lookup.
- Removed arbitrary source refs from version one and corrected Taskboard counter
  semantics: the counter validates the selected lineage; it is not a global
  cross-branch reservation.
- Required independent clean-state checks for Studio, game, and engine.
- Made engine initialization exact, local-object-only, no-fetch, and
  command-scoped so persistent Git config is not changed.
- Derived destructive targets from canonical base plus validated name; manifest
  paths are assertions, not deletion targets.
- Reclassified port assignments as advisory leases and added safe reallocation.
- Split committed Git-tree Taskboard validation from live filesystem validation.

### Cycle 2 — REVISE

Resolved findings:

- Protected the nested private game and engine from Studio's broad ignored-path
  view while allowing unrelated Studio ignored output to be cleaned.
- Published the active record before workspace-directory and manifest creation,
  closing pre-manifest orphan states.
- Made removal's active record the recovery authority through port release and
  atomic tombstone transition.
- Replaced an assumed live-game smoke with a deterministic disposable private
  game created from `templates/template`, committed fixture tasks, explicit
  DevAPI probes, and per-workspace web sentinels.

### Cycle 3 — REVISE

Resolved findings:

- Made `recover` state-aware: creation rolls back, removal resumes forward even
  after `workspace.json` has been removed.
- Narrowed nested-root protection to candidates that equal, contain, or are
  contained by the game/engine roots; unrelated Studio siblings remain eligible
  for cleanup.
- Added tests for Studio reporting `games/private/` as ignored while a separate
  Studio build directory is successfully cleaned.

### Cycle 4 — PASS

Resolved the last journal-order finding by persisting `removing` and
`transactionMode:remove` before the first destructive cleanup, then requiring
an atomic journal update at every subsequent boundary.

## Final reviewer conclusions

- Plan checker: PASS.
- Git/worktree safety reviewer: PASS.
- Remaining HIGH: 0.
- Remaining MEDIUM: 0.

Implementation probing established one Git constraint missed by the paper
review: a linked Studio worktree that initialized a submodule cannot be removed
after deinit without the single `--force` form. The reviewed plan now permits
that exact post-clean-gate exception while still forbidding force-removal of the
game worktree and Git's double-force lock override.

## Implementation review

The implemented CLI then passed two independent read-only review tracks after
revision. The fixes added immutable Git-identity checks to every destructive
recovery path, per-repository ignored-output allowlists, pre-canonicalization
junction rejection, bounded concurrent locking, stale-lock recovery ownership,
active-record-first port/tombstone transactions, and failure injection at every
persisted creation boundary. The final suite covers concurrent creation,
external port races, dirty and ignored data preservation, stale locks, junctions,
tampered records, and interrupted creation/removal.

## Verification coverage

Grounded existing references:

- `ai_studio/workspace/catalog.mjs`: workspace/private-game discovery.
- `ai_studio/workspace/games.mjs`: private-game preflight.
- `ai_studio/taskboard/store.mjs` and `stores.mjs`: task-store parsing,
  selection, counters, and validation.
- `games/new_game.mjs`: deterministic private-game fixture creation.
- `games/private/<id>/cmake/GameOptions.cmake`: nested path and engine lookup
  contract.
- `templates/template/src/main.c`: `--devapi` and `--fresh-state` launch
  options used by the deterministic smoke fixture.
- `games/private/<id>/tools/serve_web.mjs`: explicit web port and directory.
- `ai_studio/studio.mjs`: `verify --domain workspace` quality command.

New symbols and files in the plan are intentionally unverified implementation
artifacts: `feature_workspaces.mjs`, its tests, registry/manifest schemas, and
the new CLI flags.

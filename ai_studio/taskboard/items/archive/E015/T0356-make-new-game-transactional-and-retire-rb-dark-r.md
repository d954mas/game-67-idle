---
id: T0356
title: Make new_game transactional and retire rb-dark-rpg cleanly
status: done
project: P001
epic: E015
priority: P0
tags: [new-game, transaction, cleanup]
created: 2026-07-10
updated: 2026-07-11
---

## What

Make game creation a recoverable transaction and remove the abandoned
`rb-dark-rpg` prototype instead of migrating it. Identity must be transformed
from the new game's `game.json`, with no template-name fallback left behind.

## Done when

- [x] `new_game` writes into a staging directory, validates the complete result,
      then performs one atomic final rename; failures clean the staging tree.
- [x] Existing destinations fail safely unless explicit `--replace` is used;
      replacement also has rollback and cannot leave a half-created game.
- [x] Game id, title, storage namespace, generated paths, build metadata, tests,
      and IDE wiring all derive from the new `game.json`.
- [x] Tests cover stale destination files, invalid identity, late generator
      failure, replacement rollback, and unique test storage namespaces.
- [x] `rb-dark-rpg` and its catalog/task/IDE/report references are deleted; no
      migration or compatibility entry is created.
- [x] Asset references to `rb-dark-rpg` are inventoried and removed or replaced
      with sanitized reusable fixtures.
- [x] External Canvas projects retain their `gameId`, receive an archive marker,
      and disappear from the active view; shared project data is not deleted.

## Open questions

None.
## Log

- 2026-07-10: The lead explicitly chose deletion of `rb-dark-rpg` and rejected
  migrating it as an active game.
- 2026-07-10: Execution dependency: T0355 must land first so new_game has one identity and Workspace mount contract.
- 2026-07-10: Resolved planning detail: replacement uses same-volume sibling staging/backup, atomic renames, restore-on-failure, and refuses non-atomic cross-volume replacement.
- 2026-07-11: Checkpoint: started after T0355 and T0371. Scope is transactional games/new_game creation, clean rb-dark-rpg retirement, and external Canvas archive visibility only. Preserving all existing T0393 audio/template WIP and the second agent's games/web-dressup work; no broad staging or cleanup.
- 2026-07-11: TDD evidence: baseline games/new_game.test.mjs 19/19; transactional RED subset 4/9 pass with five expected failures for --replace, identity transforms, and late-failure rollback; first GREEN 29/29 before review fixes. Canvas archive RED 69/73 with four expected failures, then GREEN 73/73.
- 2026-07-11: External retirement evidence: deleted the clean 561-file games/rb-dark-rpg tree; six shared Canvas projects were updated in place to archived=true with ownership.gameId preserved. Default owner list returns 0 active projects; --include-archived returns exactly six archived projects. No Canvas project data was deleted.
- 2026-07-11: Review cycle 1: architecture/process reviewers found cross-visibility replacement, fail-open identity transforms, concurrent rollback clobbering, storage-namespace collision, missing live Canvas archive, and one misleading retired-doc reference. Canvas/archive and doc findings are fixed; transactional fixes are in progress. Dress-up and T0393 overlays remain excluded.
- 2026-07-11: Review cycles 2-3: added cross-process candidate-published claim locking, real same-id/shared-namespace race tests, exact Taskboard CAS rollback, post-commit best-effort backup cleanup, independent rollback phases, and safe abandoned-candidate cleanup. Final independent recheck converged at current_high=0 and current_actionable=0 for both architecture and process reviewers.
- 2026-07-11: Final evidence: games/new_game.test.mjs 41/41; Canvas store/ops/API/CLI 73/73; Items Viewer/Workspace/VS Code 42/42; Architecture Map 23/23 plus strict mapped=352 with all issue counts zero; Taskboard and doc-reference validation green; git diff checks clean. The second agent's games/web-dressup changes and T0393 audio/template overlay were excluded by exact allow-list.
- 2026-07-11: Quality: QTECH_001=pass; evidence: Transactional CLI behavior exercised by 41 focused tests including Windows cleanup, rollback, and real concurrency races; Canvas archive behavior 73/73; integration validators green; two independent reviews 0 HIGH and 0 actionable.

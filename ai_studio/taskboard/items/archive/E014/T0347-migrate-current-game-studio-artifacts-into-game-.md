---
id: T0347
title: Migrate current game Studio artifacts into game stores
status: done
project: P001
epic: E014
priority: P1
tags: [migration, rb-dark-rpg, taskboard, canvas]
created: 2026-07-09
updated: 2026-07-10
---

## What

Migrate current game-specific Studio artifacts into game-owned `.ai_studio/`
stores after privacy and mount-aware gates are implemented.

`rb-dark-rpg` is the first migration target. This task must not start file moves
until `T0342`, `T0344`, `T0345`, and relevant generator hardening from `T0346`
are done.

## Done when

- [x] The lead's deletion decision and successor ownership in E015 T0356 are
      recorded explicitly.
- [x] The useful `rb-dark-rpg` inventory and completed 46-file Taskboard move
      evidence remain preserved in this card.
- [x] Remaining deletion, reference cleanup, and external Canvas archive-marker
      ownership is transferred to T0356 without claiming migration completion.
- [x] Shared external Canvas data and product files are not changed by this
      cancellation.
- [x] Taskboard validation passes after the card is archived as superseded.

## Open questions

None. The clean-deletion decision supersedes migration; historical Canvas
storage policy remains preserved in the log below.

## Log

- 2026-07-11: T0375 cancellation: superseded by E015 T0356 after the lead chose clean deletion of the closed `rb-dark-rpg` prototype instead of completing its migration. Preserve the inventory and partial Taskboard/Canvas evidence below; do not execute the remaining migration criteria.
- 2026-07-11: Quality: skip; reason: intentionally cancelled ownership reconciliation with no product implementation.

- 2026-07-09: Created as final child task from `T0341` review. Migration is
  intentionally behind the privacy, Taskboard, Canvas, and generator gates.
- 2026-07-09: T0345 closure review clarified a migration caveat: Canvas
  `chat/` lives under the project folder, but undo/history sidecars live in a
  per-machine local cache keyed by projects root. T0347 owns deciding whether to
  preserve that cache during current-game migration or log intentional loss.
- 2026-07-09: Started with Taskboard-only migration slice. Inventory confirms
  `rb-dark-rpg` has one project (`P003`), three epics (`E011`, `E012`, `E013`),
  28 active tasks, and 14 archived tasks in the parent Studio store. Canvas
  migration is separate because the configured Canvas projects root is outside
  the repo (`canvasProjectsRoot`) and needs an explicit move/copy policy.
- 2026-07-09: Moved all 46 `rb-dark-rpg` Taskboard files into
  `games/rb-dark-rpg/.ai_studio/taskboard/items/`. Verified parent Studio query
  for `--project P003` returns no tasks, `--game rb-dark-rpg --all --archive`
  returns the moved project/epics/tasks with `game:rb-dark-rpg:*` qualified ids,
  parent Taskboard validate passes, and direct game-store validation reports
  zero problems.
- 2026-07-09: Canvas/evidence migration audit found two current Canvas sources:
  the configured YandexDisk Studio projects root has three `rb-dark-rpg`
  projects, and the legacy repo-local ignored root has three more. The initial
  game-local Canvas scaffold was removed after the lead clarified that raw
  Canvas stays in the shared external store; parent ignore rules now defensively
  block accidental game-local Canvas folders from entering git.
- 2026-07-09: Lead clarified the intended Canvas model: raw Canvas projects are
  too large for git and should remain in the shared external YandexDisk-backed
  store. Moved the three legacy repo-local ignored `rb-dark-rpg` Canvas
  projects into the shared `canvasProjectsRoot`; the three configured YandexDisk
  projects were already there. Follow-up correction: no game-side Canvas ref
  list is needed; each Canvas project must carry `ownership.kind/gameId`.
- 2026-07-11: T0375 status reconciliation: intentionally closed as superseded. E015 T0356 owns clean rb-dark-rpg deletion; remaining migration work must not run. Existing inventory and partial migration evidence are preserved.

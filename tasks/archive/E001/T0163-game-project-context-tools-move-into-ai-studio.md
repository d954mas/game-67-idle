---
id: T0163
title: Game project context tools move into AI Studio
status: done
epic: E001
priority: P2
tags: [game-project, legacy, ai-studio]
created: 2026-06-30
updated: 2026-06-30
---

## What

Move the reviewed game-project context tools out of legacy `tools/` and into AI
Studio ownership. These tools own active game routing and prototype kickoff
context, so they should live near the rest of the harness instead of remaining
as generic repo scripts.

## Done when

- [x] `new_prototype.mjs`, `iteration_context.mjs`, and their tests live under
      an AI Studio module with a short README.
- [x] Commands, tests, bootstrap/export copy list, and docs reference the new
      path.
- [x] Architecture map shows the new owner and no longer lists
      `tools/game_context` as the current module.
- [x] Existing behavior is preserved by focused tests.
- [x] Taskboard, architecture map, doc references, and skills sync validate.

## Open questions

- Later: decide whether `primary-gdd-pipeline` belongs under this module or a
  separate design/GDD module. This task does not move skills.

## Log

- 2026-07-01: Review found `tools/game_context` is the current owner of
  `GAME_PROJECT.md` routing, prototype kickoff skeletons, and startup context
  gate. That is AI Studio harness behavior, not a generic legacy tool.
- 2026-07-01: Moved `tools/game_context` to `ai_studio/game_project`, added the
  module README, updated usage paths/tests/export docs, and moved
  `GAME_PROJECT.md` map ownership from Core Harness to Game Project.
- 2026-07-01: Validation passed: game_project + export_base tests 8/8,
  architecture map validation clean, taskboard validation clean, doc reference
  check clean, and skills sync clean.
- 2026-06-30: 2026-07-01: Closed after moving Game Project context tools into ai_studio/game_project and validating focused tests, export, map, taskboard, doc refs, and skills sync.

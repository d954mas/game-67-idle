---
id: T0171
title: Refactor game feature iteration workflow
status: done
epic: E001
priority: P2
tags: [game-project, skill, refactor, iteration]
created: 2026-06-30
updated: 2026-06-30
---

## What
Move the legacy `game-feature-iteration` skill into AI Studio ownership as a
small playable-iteration workflow. Keep only the useful iteration loop and
handoff/review shape; route reference research, quality checks, runtime proof,
and active-game context to their existing AI Studio modules.

## Done when

- [x] Reviewed legacy `game-feature-iteration` files and classified keep/merge/drop.
- [x] Created an `ai_studio/game_project/feature_iteration/` group with concise docs.
- [x] Replaced legacy skill surface with `nt-game-feature-iteration`.
- [x] Updated `ai_studio/tree.json` and generated agent surfaces.
- [x] Validated map, docs, skill surface, and taskboard.
- [x] Committed and pushed the slice.

## Open questions

- Resolved for now: playable iteration lives under Game Project because it works
  on the active current game. If a broader Production/Tech module appears later,
  this workflow can move there as a child.

## Log

- 2026-07-01: Started migration slice after map validation showed
  `game-feature-iteration` as unmapped legacy.
- 2026-07-01: Review found useful core: orient to active game, choose one
  player-visible goal, implement the smallest playable slice, validate primary
  runtime, capture evidence, review as product, update durable state when useful,
  commit intentionally. Old references duplicate GDD/reference/quality/runtime
  modules and use stale `tools/game_context` paths.
- 2026-07-01: Implemented as `ai_studio/game_project/feature_iteration/` plus
  `nt-game-feature-iteration`. Kept the iteration loop and playable gates;
  routed GDD, reference, quality, and runtime proof to their existing AI Studio
  modules.
- 2026-07-01: Validation passed: skills surface sync, architecture map, doc
  references, taskboard validation, Game Project tests, and local skill
  frontmatter check. Map now reports `unmapped_legacy=16`.
- 2026-07-01: Moved playable feature iteration workflow into
  `ai_studio/game_project/feature_iteration`, replaced legacy skill with
  `nt-game-feature-iteration`, synced surfaces, and validated
  map/docs/taskboard/tests.

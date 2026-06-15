---
id: T0023
title: Make passive profiling review-usable for current slices
status: done
epic: E003
priority: P1
tags: []
created: 2026-06-15
updated: 2026-06-15
---

## What

Fix the profiling failure exposed by the Splash Rods review: the passive
profile had near-zero wall-clock coverage, so review conclusions could not
identify the real bottlenecks. A future slice must not be handed off as
measured when its current-scope profile is empty, stale, or too sparse.

This is a pipeline reliability task, not more fishing-game development.

## Done when

- [x] `node tools/ai.mjs status` reports current-scope review confidence
      separately from historical whole-session coverage.
- [x] A handoff/review guard can fail when current-scope profiling is not
      review-usable.
- [x] The guard tells the agent the concrete recovery command, such as
      checkpointing a long unprofiled gap or resetting scope.
- [x] Pipeline/task docs require the guard for AI workflow/profiler review
      slices before claiming profiling evidence.
- [x] Validation covers the new profile health behavior.

## Open questions

- Should the guard live inside `status`, a new `health` command, or the
  existing `close-slice`/handoff path?

## Log

- 2026-06-15: Created after the lead confirmed `node tools/ai.mjs status`
  showed only about 3.3% coverage during the fishing review. Current observed
  status after cleanup was still low for the whole session: 1.5% coverage and
  `Review confidence: partial`.
- 2026-06-15: Added `current_scope_review_confidence` to
  `tools/ai_profile/status.mjs` and a guard flag:
  `node tools/ai.mjs status --require-current-scope-usable`.
- 2026-06-15: Guard now fails start-only/current-scope-too-shallow profiles
  and prints a concrete recovery command:
  `node tools/ai.mjs checkpoint "<intent>" --force`.
- 2026-06-15: Updated `AI_PIPELINE.md` and `tasks/README.md` so AI workflow,
  profiler, retrospective, and pipeline-review slices must pass the
  current-scope guard before claiming profiling evidence.
- 2026-06-15: Validation passed:
  `node --test tools/ai_profile/test.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `node tools/pipeline_validate.mjs`.
- 2026-06-15: Real current scope check passed:
  `node tools/ai.mjs status --require-current-scope-usable` reported
  `Current scope review confidence: usable`, while the whole historical
  profile remained `partial` because old coverage was still low.

---
id: T0015
title: Flag stale profiling scopes
status: done
epic: E003
priority: P0
tags: [profiling, tooling, taskboard]
created: 2026-06-15
updated: 2026-06-15
---

## What

Make `node tools/ai.mjs status` detect when the current profiling scope points
to an archived/done/dropped task. This prevents stale scopes from silently
making old prototype work look current after a project iteration is closed.

## Done when

- [x] Status JSON includes task-store metadata for the current scope.
- [x] Passive status output marks archived/done/dropped scope tasks as stale.
- [x] Next action asks the agent to reset/start profiling for active work when
      the current scope is stale.
- [x] Tests cover stale archived scope detection.
- [x] Real `node tools/ai.mjs status` points at current active work after
      resetting scope.

## Open questions

- none

## Log

- 2026-06-15: `node tools/ai.mjs status` previously showed
  `T0010/fishing-native-prototype` after the fishing task was archived and
  still said no profiling maintenance was needed.
- 2026-06-15: Added task-store lookup to `tools/ai_profile/status.mjs`,
  including `scope_task`, `stale_scope`, passive output labeling, and a reset
  next action for archived/done/dropped scopes.
- 2026-06-15: Added regression coverage in `tools/ai_profile/test.mjs` with
  `AI_PROFILE_TASK_ROOT` so stale-scope tests are portable.
- 2026-06-15: Reset current profile scope to `T0015/stale-profile-scope-cleanup`
  with `node tools/ai.mjs start T0015 stale-profile-scope-cleanup --phase pipeline_cleanup --category tooling ...`.
  Verification: `node --test tools/ai_profile/test.mjs` passes 69 tests, and
  `node tools/ai.mjs status` now shows current scope task `doing` for T0015.
- 2026-06-15: Done: ai status now flags archived/done/dropped profile scopes as stale; tests pass and current scope was reset to active work.

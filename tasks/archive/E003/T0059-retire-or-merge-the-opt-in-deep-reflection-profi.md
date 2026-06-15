---
id: T0059
title: Retire or merge the opt-in deep-reflection profiler chain
status: done
epic: E003
priority: P2
tags: [profiling, subtraction, ai-workflow]
created: 2026-06-15
updated: 2026-06-15
---

## What

`tools/ai_profile/` is ~5,600 LOC source + ~3,574 LOC of `test.mjs`. After T0044
(passive profiling) and T0047 (planner removed), the passive facade defaults
(`start/run/context/checkpoint/summary/status/reflect-short`) never invoke the
heavy deep-reflection chain. Verified: `ai reflect` (no `--deep`) runs
`closeout --no-review --no-followups` then `prepare_reflection.mjs`, which exits
early with a "no baseline" warning unless someone ran `capture_baseline.mjs`
(no baseline exists). So `review.mjs` (1212), `reflection_packet.mjs` (230),
`reflection_draft.mjs` (567), `reflection_review.mjs` (552),
`compare_reviews.mjs` (217), `capture_baseline.mjs` (95), `followups.mjs` (301)
-- ~3,150 LOC + ~38 test cases -- are reachable ONLY via opt-in baseline +
non-`--no-review` closeout. This is the largest remaining subtraction candidate.

NOTE: `event.mjs` and `scope.mjs` are NOT orphaned -- they are pervasive test
fixtures (46 / 16 test invocations). Do not delete them under this task.

## Done when

- [x] Lead decision: baseline retrospectives are NOT used and are conceptually unfit (each session is a different game/task, so cross-session review baselines are not comparable -- it is a benchmark/CI model). Full delete chosen.
- [x] Deleted 8 files (3382 LOC): review, followups, capture_baseline, compare_reviews, reflection_packet, reflection_draft, reflection_review, prepare_reflection. Rewired `ai.mjs reflect` -> short closeout only (`--deep` is a retired no-op); stripped review/followups from `closeout.mjs`; removed baseline/compare/reflection modeling from `status.mjs` (kept the passive core + guard flags). `event.mjs`/`scope.mjs` kept (live fixtures).
- [x] Passive defaults unaffected; `ai status`/`status --verbose`/short `reflect` work; ai.test 14/14, ai_profile/test 71->23 blocks all pass, quick pipeline_validate pass, taskboard ok.

## Open questions

- RESOLVED: full delete. Sessions are different games/tasks, so baseline comparison is apples-to-oranges; this-session health via `ai status` + short `reflect` is what's actually useful.

## Log

- 2026-06-15: Captured from the second simplification/speed iteration as the dominant remaining subtraction. Needs the lead's answer on baseline retrospectives before cutting.

- 2026-06-15: Lead confirmed baseline retrospectives are unused + conceptually unfit. Deleted the 8-file deep-reflection chain (3382 LOC); rewired reflect (short closeout only, --deep retired no-op), closeout (no review/followups), status (passive core only, guards kept). ai_profile/test 71->23 blocks. Passive defaults verified working; quick validate + taskboard ok.

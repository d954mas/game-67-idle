---
id: T0059
title: Retire or merge the opt-in deep-reflection profiler chain
status: backlog
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

- [ ] Lead decision recorded: are baseline-anchored deep retrospectives still wanted?
- [ ] If NOT wanted: delete review/packet/draft/review/compare/baseline/followups + their tests, and shrink the baseline/comparison modeling in `status.mjs` (~250 LOC at :191-320). If wanted-but-heavy: collapse packet -> draft -> review (a strict linear pipeline) into one `reflect_deep.mjs`.
- [ ] Passive defaults (start/run/context/checkpoint/summary/status/reflect-short) unaffected; `node --test tools/ai.test.mjs` + `node --test tools/ai_profile/test.mjs` + `node tools/pipeline_validate.mjs` pass.

## Open questions

- LEAD: do you ever run baseline-anchored retrospectives (`capture_baseline` then `reflect --deep`/compare)? If no, full delete (~3,000 LOC source + ~1,500 test LOC). If rarely, merge the linear trio.

## Log

- 2026-06-15: Captured from the second simplification/speed iteration as the dominant remaining subtraction. Needs the lead's answer on baseline retrospectives before cutting.

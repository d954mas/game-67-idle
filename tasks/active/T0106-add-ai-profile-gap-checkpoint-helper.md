---
id: T0106
title: Add AI profile gap checkpoint helper
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, coverage, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a thresholded wall-clock checkpoint helper for AI profiling. It should
record long manual/research/review gaps without adding noise for short pauses.

## Done when

- [x] `tools/ai_profile/gap_checkpoint.mjs` appends a checkpoint only when the
      elapsed gap since the latest profile record is at least `--min-gap-min`.
- [x] The helper stores raw gap, threshold, cap status, previous record
      metadata, work-item scope, and inferred duration.
- [x] Short gaps are skipped without writing a profile record.
- [x] Regression tests cover append and skip behavior.
- [x] Profiling docs and reflection skill mention when to use the helper.
- [x] Full validation is complete and the work is ready to commit.

## Open questions

None.

## Log

- 2026-06-13: Started `T0106` scope with `tools/ai_profile/start.mjs`.
- 2026-06-13: Added `gap_checkpoint.mjs`, tests, validation-plan coverage,
  docs, skill rule, and iteration-log entry.
- 2026-06-13: Early validation passed: `node --check
  tools/ai_profile/gap_checkpoint.mjs` and `node --test
  tools/ai_profile/test.mjs` with 55 tests.
- 2026-06-13: Real validation batch passed through
  `node tools/ai_profile/validation_run.mjs --change profiling --change
  pipeline --change skills --risk medium --batch-id T0106-gap-checkpoint
  --json-output tmp/session_profiles/validation_run_T0106.json`; 9 checks
  passed, 1 placeholder skipped, and the final broad gate ran once.

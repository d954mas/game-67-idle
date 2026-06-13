---
id: T0108
title: Keep historical repeated validation out of current reflection actions
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Keep historical repeated broad/final validation from becoming a pending
current reflection action when the current scope only contains planned batched
final gates.

## Done when

- [x] `followups.mjs` uses `current_scope.repeated_unbatched_broad_final_commands`
      for current broad/final validation action decisions.
- [x] Backward compatibility remains for older review JSON that only has
      `repeated_broad_final_commands`.
- [x] Regression test covers historical unbatched repeats plus current-scope
      batched-only repeats.
- [x] Reflection rules/docs explain that batched current-scope final gates are
      validation evidence, not a pending action.
- [x] Full validation is complete and the work is ready to commit.

## Open questions

None.

## Log

- 2026-06-13: Started `T0108` scope with `tools/ai_profile/start.mjs`.
- 2026-06-13: Updated `followups.mjs` to use unbatched broad/final repeats for
  current-scope follow-up decisions and added a regression test.
- 2026-06-13: Early validation passed: `node --check
  tools/ai_profile/followups.mjs` and `node --test tools/ai_profile/test.mjs`
  with 59 tests.
- 2026-06-13: Full profiled validation passed with
  `node tools/ai_profile/validation_run.mjs --change profiling --change
  pipeline --change skills --risk medium --batch-id
  T0108-historical-reflection-action-scope --json-output
  tmp/session_profiles/validation_run_T0108.json`; 9 checks passed, 1
  placeholder skipped, and the final broad gate ran once.
- 2026-06-13: `prepare_reflection.mjs` regenerated fresh handoff artifacts;
  `reflection_review.json` now reports `verdict: current_clean`, no pending
  follow-ups, and historical repeated broad/final validation is suppressed.

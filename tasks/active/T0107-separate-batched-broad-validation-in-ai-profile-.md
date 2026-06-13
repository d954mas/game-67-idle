---
id: T0107
title: Separate batched broad validation in AI profile review
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, validation, reflection, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Separate planned validation-batch final gates from ad hoc repeated broad/final
validation in AI profile review and reflection artifacts.

## Done when

- [x] `review.mjs` reports `batched_broad_final_commands` and
      `repeated_unbatched_broad_final_commands` in JSON.
- [x] `review.mjs` uses unbatched repeated broad/final commands, not total
      broad/final count, for likely-waste findings and actions.
- [x] Current-scope repeated broad/final findings ignore planned batch final
      gates.
- [x] `reflection_draft.mjs` and `reflection_review.mjs` carry the
      batched/unbatched distinction.
- [x] `compare_reviews.mjs` treats repeated batched broad/final commands as
      validation evidence, not current-scope regression.
- [x] Regression tests cover mixed, batched-only, and draft propagation cases.
- [x] Full validation is complete and the work is ready to commit.

## Open questions

None.

## Log

- 2026-06-13: Started `T0107` scope with `tools/ai_profile/start.mjs`.
- 2026-06-13: Added batched/unbatched broad-final classification to review,
  reflection draft/review propagation, tests, docs, skill rule, and iteration
  log entry.
- 2026-06-13: Early validation passed: `node --check` for touched profiler
  scripts and `node --test tools/ai_profile/test.mjs` with 57 tests.
- 2026-06-13: Full profiled validation passed with
  `node tools/ai_profile/validation_run.mjs --change profiling --change
  pipeline --change skills --risk medium --batch-id
  T0107-broad-validation-classification --json-output
  tmp/session_profiles/validation_run_T0107.json`; 9 checks passed, 1
  placeholder skipped, and the final broad gate ran once.
- 2026-06-13: Found and fixed a remaining false regression in
  `compare_reviews.mjs`; baseline compare now uses unbatched broad/final
  repeats with a backward-compatible fallback.
- 2026-06-13: Reran full profiled validation after the compare fix; 9 checks
  passed, 1 placeholder skipped, and `prepare_reflection.mjs` generated fresh
  packet, draft, and review artifacts without baseline regressions.

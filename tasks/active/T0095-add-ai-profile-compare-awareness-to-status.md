---
id: T0095
title: Add AI profile compare awareness to status
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Teach `status.mjs` to detect whether the current fresh review JSON has already
been compared against the latest captured baseline, and to show the exact
compare command when comparison evidence is missing or stale.

## Done when

- [x] `status.mjs` reports latest baseline comparison path/status when a
      captured baseline exists.
- [x] If bundle is fresh and comparison is missing or stale, status next action
      prints an exact `compare_reviews.mjs` command using the latest baseline
      and current review JSON.
- [x] If comparison JSON exists and has current-scope regressions, status next
      action tells the agent to inspect regressions before reflection.
- [x] If comparison JSON exists and is fresh with no current-scope regressions,
      status treats it as ready baseline trend evidence.
- [x] Regression tests cover missing, stale, regressed, and fresh comparison
      status behavior.
- [x] Profiling docs/reflection skill mention checking comparison status before
      writing trend claims.

## Open questions

## Log

- 2026-06-13: `status.mjs` reports captured baselines but only shows a compare
  command template with `<current.review.json>`. It does not tell agents whether
  the current review has already been compared, so reflection can still repeat
  comparison setup or miss a regression artifact.
- 2026-06-13: Implemented comparison status detection in `status.mjs`
  (`missing`, `stale`, `regressed`, `fresh`), added exact compare next-action
  generation, covered all four states in tests, updated profiling docs and
  reflection skill/eval anchors, and validated with
  `node --test tools/ai_profile/test.mjs`, `node tools/skills_eval.mjs`,
  `node tools/taskboard/cli.mjs validate`, `git diff --check`, and
  `node tools/pipeline_validate.mjs`.
- 2026-06-13: Baseline comparison status awareness implemented and validated.

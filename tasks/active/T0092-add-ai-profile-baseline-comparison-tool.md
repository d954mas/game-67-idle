---
id: T0092
title: Add AI profile baseline comparison tool
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a low-overhead AI profile review comparison tool so future reflections can
compare a later `review.json` against a clean baseline without manually reading
two large artifacts.

## Done when

- [x] `tools/ai_profile/compare_reviews.mjs` compares two review JSON files and
      prints markdown with current-scope regressions, improvements, deltas, and
      a verdict.
- [x] The compare tool can write machine-readable JSON for automation or later
      agents.
- [x] The compare tool has regression tests for clean improvement and
      current-scope regression cases.
- [x] Profiling docs and reflection skill explain when to compare against a
      baseline review.
- [x] Skill eval covers the new comparison guidance.

## Open questions

## Log

- 2026-06-13: Current followups reported a clean baseline but only said to
  compare a later profile against it. Without a compare tool, future agents
  would need to manually reconcile two review JSON files during reflection.
- 2026-06-13: Implemented `tools/ai_profile/compare_reviews.mjs`, added tests
  for improvement and current-scope regression with `--fail-on-regression`,
  updated profiling docs/reflection skill/eval anchors, and validated with
  `node --test tools/ai_profile/test.mjs`, `node tools/skills_eval.mjs`,
  `node tools/taskboard/cli.mjs validate`, `git diff --check`, and
  `node tools/pipeline_validate.mjs`.
- 2026-06-13: Baseline comparison tool implemented and validated.

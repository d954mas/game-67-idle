---
id: T0094
title: Add AI profile baseline awareness to status
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Teach `status.mjs` to detect captured profile baselines so agents do not keep
repeating baseline capture after a clean profile has already been preserved.

## Done when

- [x] `status.mjs` reports baseline manifest count and latest captured
      baseline path when `tmp/session_profiles/baselines/*.manifest.json`
      exists.
- [x] Status next action recommends `capture_baseline.mjs` only when the
      profile is otherwise clean and no captured baseline exists.
- [x] Status next action points to the latest captured baseline when one
      exists.
- [x] Regression tests cover missing-baseline and existing-baseline status
      behavior.
- [x] Profiling docs and reflection skill mention checking status baseline
      awareness before recapturing.

## Open questions

## Log

- 2026-06-13: After adding `capture_baseline.mjs`, `status.mjs` still reported
  "Use this profile as baseline" with no indication whether a baseline was
  already captured. That can cause repeated baseline capture loops during
  resumed reflection work.
- 2026-06-13: Implemented baseline manifest detection in `status.mjs`, added
  status next-action tests for missing/present baselines, updated profiling
  docs/reflection skill/eval anchors, and validated with
  `node --test tools/ai_profile/test.mjs`, `node tools/skills_eval.mjs`,
  `node tools/taskboard/cli.mjs validate`, `git diff --check`, and
  `node tools/pipeline_validate.mjs`.
- 2026-06-13: Baseline-aware profile status implemented and validated.

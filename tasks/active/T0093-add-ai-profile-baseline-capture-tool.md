---
id: T0093
title: Add AI profile baseline capture tool
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a small baseline capture tool so a clean `review.json` can be preserved
under a stable scratch path before later closeout/review commands overwrite the
same daily review artifact.

## Done when

- [x] `tools/ai_profile/capture_baseline.mjs` copies a review JSON to a
      stable baseline path with a safe label.
- [x] The tool writes a machine-readable manifest with source, target, label,
      captured time, current-scope counts, and compare command.
- [x] The tool refuses overwrite unless `--force` is passed.
- [x] Regression tests cover capture, manifest contents, and overwrite guard.
- [x] Followups/docs/reflection skill tell agents to capture a clean baseline
      before comparing later profiles.
- [x] Skill eval covers the baseline capture guidance.

## Open questions

## Log

- 2026-06-13: Followups said to keep the clean review JSON as a baseline, but
  daily closeout/review artifacts use stable filenames that can be overwritten.
  Without a baseline capture helper, future agents can lose the comparison
  anchor or spend time reconstructing which review was meant to be baseline.
- 2026-06-13: Implemented `tools/ai_profile/capture_baseline.mjs`, added
  regression tests for manifest capture and overwrite refusal, updated
  followups/docs/reflection skill/eval anchors, smoke-tested live baseline
  capture to `tmp/session_profiles/baselines/current-clean-profile.*`, and
  validated with `node --test tools/ai_profile/test.mjs`,
  `node tools/skills_eval.mjs`, `node tools/taskboard/cli.mjs validate`,
  `git diff --check`, and `node tools/pipeline_validate.mjs`.
- 2026-06-13: Baseline capture tool implemented and validated.

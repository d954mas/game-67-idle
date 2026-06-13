---
id: T0103
title: Add AI reflection review handoff status
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Make `reflection_review` the final automatic handoff artifact for AI
development retrospectives. `status.mjs` should report review freshness and
the exact regeneration command, while `prepare_reflection.mjs` should generate
the review after a fresh draft.

## Done when

- [x] `tools/ai_profile/status.mjs` reports reflection review
      missing/stale/waiting/fresh state and uses a fresh review as the final
      next-action artifact.
- [x] `tools/ai_profile/prepare_reflection.mjs` generates missing or stale
      reflection review artifacts after packet and draft are fresh.
- [x] Regression tests cover missing, waiting, fresh, no-op, and generated
      review handoff cases.
- [x] Reflection profiling docs, skill rules, and skill eval anchors mention
      packet/draft/review freshness.
- [x] Full validation is complete and the work is ready to commit.

## Open questions

None.

## Log

- 2026-06-13: Started `T0103` scope with `tools/ai_profile/start.mjs`.
- 2026-06-13: Added `reflection_review` paths/status/commands to
  `status.mjs`, extended `prepare_reflection.mjs`, updated tests and docs.
- 2026-06-13: Validation passed: `node --test tools/ai_profile/test.mjs`,
  `node tools/skills_eval.mjs`, `node tools/taskboard/cli.mjs validate`,
  `node tools/skills_sync.mjs`, `git diff --check`, and
  `node tools/pipeline_validate.mjs`.

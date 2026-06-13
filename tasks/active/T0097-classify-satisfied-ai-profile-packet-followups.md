---
id: T0097
title: Classify satisfied AI profile packet followups
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Make `reflection_packet.mjs` distinguish pending follow-up drafts from
follow-ups that are already satisfied by current scratch evidence, starting
with the clean-baseline suggestion after a captured baseline and fresh
comparison already exist.

## Done when

- [x] Reflection packet JSON separates `pending_suggestions` and
      `satisfied_suggestions`.
- [x] The clean-baseline follow-up is marked satisfied when a baseline exists
      and comparison is fresh with no current-scope regressions.
- [x] Packet markdown shows satisfied follow-ups separately from pending
      follow-ups.
- [x] Regression tests cover satisfied baseline follow-up and pending baseline
      follow-up when comparison is missing.
- [x] Profiling docs/reflection skill tell agents not to promote satisfied
      packet follow-ups into tasks.

## Open questions

## Log

- 2026-06-13: The reflection packet reported readiness `ready` and comparison
  `stable`, but still listed "Use clean AI profile as baseline" as a follow-up
  action. That can cause agents to repeat already-completed baseline capture
  and comparison work during reflection.
- 2026-06-13: Implemented pending/satisfied follow-up classification in
  `reflection_packet.mjs`, added tests for satisfied baseline follow-up and
  pending comparison-missing follow-up, updated profiling docs/reflection
  skill/eval anchors, and validated with `node --test tools/ai_profile/test.mjs`,
  `node tools/skills_eval.mjs`, `node tools/taskboard/cli.mjs validate`,
  `git diff --check`, and `node tools/pipeline_validate.mjs`.
- 2026-06-13: Reflection packet follow-up satisfaction classification implemented and validated.

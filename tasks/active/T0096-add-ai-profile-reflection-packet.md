---
id: T0096
title: Add AI profile reflection packet
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a compact reflection packet generator that gathers the current profile
status, review JSON, follow-up drafts, and baseline comparison into one
scratch markdown/JSON artifact before writing a retrospective.

## Done when

- [x] `tools/ai_profile/reflection_packet.mjs` reads a profile's generated
      review/followup/baseline comparison artifacts by convention.
- [x] The packet reports current-scope findings/actions, follow-up suggestions,
      suppressed historical findings, baseline comparison verdict/regressions,
      and source artifact paths.
- [x] The packet writes markdown and machine-readable JSON.
- [x] Regression tests cover clean packet output and regressed comparison
      output.
- [x] Profiling docs and reflection skill tell agents to generate/read the
      packet before writing a full retrospective when profile artifacts exist.

## Open questions

## Log

- 2026-06-13: Status, review, followups, baseline capture, and comparison now
  exist, but reflection still requires manually opening multiple scratch
  artifacts. A compact packet can reduce context load and make retrospectives
  more repeatable.
- 2026-06-13: Implemented `tools/ai_profile/reflection_packet.mjs`, added
  clean/regressed packet regression tests, updated profiling docs and
  reflection skill/eval anchors, and validated with
  `node --test tools/ai_profile/test.mjs`, `node tools/skills_eval.mjs`,
  `node tools/taskboard/cli.mjs validate`, `git diff --check`, and
  `node tools/pipeline_validate.mjs`.
- 2026-06-13: Reflection packet tool implemented and validated.

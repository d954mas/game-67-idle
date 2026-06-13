---
id: T0102
title: Add AI reflection review generator
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a small review generator that consumes `reflection_draft.mjs --json-output`
and emits a compact current reflection review: current-state verdict,
historical-only lessons, repeated-command interpretation, and top next-cycle
improvements.

## Done when

- [x] `tools/ai_profile/reflection_review.mjs` reads a reflection draft JSON.
- [x] The review separates current action items from historical lessons that
      should not become new tasks.
- [x] The review includes a top-10 improvement list for the next AI-development
      cycle.
- [x] The tool writes markdown and machine-readable JSON.
- [x] Regression tests cover clean current scope and dirty current scope.
- [x] Profiling docs/reflection skill tell agents to run the review after
      `prepare_reflection.mjs` before writing the final retrospective.

## Open questions

## Log

- 2026-06-13: `prepare_reflection.mjs` now prepares fresh packet and draft
  artifacts, but the next step still requires manually turning the draft into
  a concise decision review. A review generator can keep current items separate
  from historical lessons and give the agent a compact top-10 improvement list
  before writing the final retrospective.
- 2026-06-13: Implemented `reflection_review.mjs`, covered clean/dirty current
  scope behavior in tests, documented it in the profiling workflow and
  reflection skill, and synced generated skills.

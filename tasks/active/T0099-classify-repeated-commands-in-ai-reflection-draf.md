---
id: T0099
title: Classify repeated commands in AI reflection drafts
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Improve the reflection draft generator so repeated command findings stop
rendering as generic "human review needed" lessons. The draft should summarize
top repeated commands by scope and turn them into specific retrospective
questions/actions.

## Done when

- [x] `tools/ai_profile/reflection_draft.mjs` includes repeated command
      evidence from review JSON in its machine-readable draft.
- [x] The markdown draft shows repeated commands by scope and top command
      examples.
- [x] The `repeated_commands` historical lesson has a specific symptom,
      cause, and fix instead of the generic fallback.
- [x] Regression tests cover scoped/preflight/broad repeated command draft
      output.
- [x] Profiling docs mention using draft repeated-command evidence to classify
      batching, justified reruns, or validation waste.

## Open questions

## Log

- 2026-06-13: The first reflection draft successfully separated current clean
  state from historical lessons, but `repeated_commands` still used the
  generic fallback cause/fix. Review JSON already contains repeated command
  scope and examples, so the draft can become more useful without expanding
  the review analyzer.
- 2026-06-13: Added repeated-command summary to `reflection_draft.mjs`, covered
  scoped/preflight/broad examples in tests, updated profiling docs and the
  reflection skill, and synced generated skills.

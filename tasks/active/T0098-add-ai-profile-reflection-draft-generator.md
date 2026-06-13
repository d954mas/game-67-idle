---
id: T0098
title: Add AI profile reflection draft generator
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a small reflection draft generator that consumes the compact reflection
packet and review JSON, then emits a structured retrospective starter with
evidence, current state, historical lessons, and next-cycle actions.

## Done when

- [x] `tools/ai_profile/reflection_draft.mjs` reads a packet JSON and the
      referenced review JSON.
- [x] The draft separates current-scope state, satisfied/pending follow-ups,
      suppressed historical lessons, whole-profile findings, and next-cycle
      actions.
- [x] The draft uses symptom/cause/fix structure for historical findings where
      possible.
- [x] The tool writes markdown and machine-readable JSON.
- [x] Regression tests cover clean packet draft and packet with pending/current
      regression draft.
- [x] Profiling docs/reflection skill tell agents to generate the draft after a
      ready packet, then edit it with judgment instead of treating it as final.

## Open questions

## Log

- 2026-06-13: The reflection packet now reduces evidence gathering, but the
  retrospective still starts from a blank page. A draft generator can preserve
  the packet's distinction between current clean state, satisfied follow-ups,
  and historical lessons while keeping the human/agent responsible for final
  judgment.
- 2026-06-13: Implemented `reflection_draft.mjs`, added two regression tests,
  documented the packet-to-draft flow in the profiling guide and reflection
  skill, and synced the skill mirror.

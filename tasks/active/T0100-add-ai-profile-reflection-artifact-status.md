---
id: T0100
title: Add AI profile reflection artifact status
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Teach `tools/ai_profile/status.mjs` to report whether the compact reflection
packet and reflection draft are missing, stale, or fresh after the bundle and
baseline comparison are ready.

## Done when

- [x] Status JSON includes `reflection.packet` and `reflection.draft` entries.
- [x] Markdown status reports reflection packet/draft state and generation
      commands.
- [x] `next_action` tells agents to generate packet or draft after fresh
      bundle/comparison instead of stopping at "use fresh comparison".
- [x] Regression tests cover missing packet, stale draft, and fresh draft.
- [x] Profiling docs/reflection skill tell agents to trust status for the
      packet/draft handoff state.

## Open questions

## Log

- 2026-06-13: After `T0098` and `T0099`, `status.mjs` still stops at fresh
  baseline comparison and does not report whether reflection packet/draft
  artifacts are missing or stale. That leaves the final pre-reflection handoff
  step in agent memory instead of a tool.
- 2026-06-13: Added reflection packet/draft status and next-action routing to
  `status.mjs`, covered missing/stale/fresh states in tests, updated profiling
  docs and the reflection skill, and synced generated skills.

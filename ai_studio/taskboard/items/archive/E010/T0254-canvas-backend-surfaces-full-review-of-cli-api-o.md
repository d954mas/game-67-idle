---
id: T0254
title: "Canvas backend surfaces: full review of CLI, API/ops layer, python tools - design, parity, performance"
status: done
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-13
quality: {"notApplicable":{"reason":"Archival cleanup of a completed historical review packet; no product behavior changes."}}
---

## What

Review Canvas backend surfaces across CLI, API/ops, and Python tools for design,
parity, correctness, and performance risks.

## Done when

- [x] CLI, API/ops, and Python-tool review packets are collected.
- [x] Synthesis identifies Tier 1 backend fixes and lead-decision questions.
- [ ] Lead-decision questions are resolved or split into follow-up tasks.

## Open questions

## Log
- 2026-07-03: All 3 dimension reviews landed (api_ops: NO cross-client write lock = top finding; cli: list overflow + python cold-start per call measured; python_tools: key_matte 2.8x free win, dead bleed+repair passes). Docs in tmp/review_T0254_*.md. Synthesis in flight.
- 2026-07-03: Synthesis landed (same doc). Tier 1 backend items launched (write lock, status codes, CLI list/bool/resize). 5 lead-decision questions parked in tmp/VERIFY_2026-07-03.md item 6.
- 2026-07-13: Archived by E018: old review narrative and synthesis were deleted after confirming current contracts/tests own live behavior; future review must remeasure current Canvas.
- 2026-07-13: Closure: waived; reason: The 2026-07 backend review snapshot is stale; implemented follow-ups already landed and any future backend review must remeasure current surfaces.; evidence: E018 live-reference audit; current CLI/API/Python behavior remains covered by owner tests and contracts.
- 2026-07-13: Quality: not-applicable; reason: Archival cleanup of a completed historical review packet; no product behavior changes.

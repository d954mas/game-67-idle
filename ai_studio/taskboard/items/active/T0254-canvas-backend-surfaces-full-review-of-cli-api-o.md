---
id: T0254
title: "Canvas backend surfaces: full review of CLI, API/ops layer, python tools - design, parity, performance"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: All 3 dimension reviews landed (api_ops: NO cross-client write lock = top finding; cli: list overflow + python cold-start per call measured; python_tools: key_matte 2.8x free win, dead bleed+repair passes). Docs in tmp/review_T0254_*.md. Synthesis in flight.
- 2026-07-03: Synthesis landed (same doc). Tier 1 backend items launched (write lock, status codes, CLI list/bool/resize). 5 lead-decision questions parked in tmp/VERIFY_2026-07-03.md item 6.

---
id: T0233
title: "Canvas: 9-slice element support (test slice-9 scaling with an image)"
status: doing
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead (2026-07-03): "сделать что-то слайс9 картинкой (чтобы проверить а
работает ли слайс9)" — he wants a 9-slice test on the canvas: give an image
slice-9 insets and verify scaling behaves (corners fixed, edges stretch one
axis, center stretches both). ai_studio/assets/viewer/ carries an old slice9
module — check what is reusable.

Phase 1 = DESIGN (deep-reasoner): minimal additive model (element.slice9
insets?), page render + PIL renderGroup parity, inspector/CLI surface,
increment plan.

## Done when

- [ ] Design doc in tmp/: reuse verdict on the old viewer slice9 module,
      additive data model, page+PIL parity plan, inspector/CLI surface,
      increments sized for fast-worker packets.
- [ ] Orchestrator review -> lead confirmation.
- [ ] Implementation increments spawned as follow-up work.

## Open questions

- Insets UI: numeric only for v1, or visual guide lines on the element?

## Log

- 2026-07-03: created from lead request during T0210 verification.
- 2026-07-03: design phase delegated to deep-reasoner (Opus).

---
id: T0235
title: "Canvas: export at on-canvas size - 'canvas' scale base alongside source pixels"
status: todo
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead (2026-07-03, export verify): "нужна возможность выгрузить как оригинал
так и размер на канвасе". Today every export scale resolves against SOURCE
pixels (element.source_w/h); the element's on-canvas w/h is ignored. Add an
export base "canvas": the row exports at the element's CURRENT on-canvas size,
resolved at export time (tracks later resizes — not frozen into a w-token).

Design:
- ops parseScaleSpec accepts token `canvas` (kind "canvas"); resolveExportScale
  gains the element's canvas w/h as context (exportElements passes them);
  result = round(element.w) x round(element.h). Loud error if element w/h
  missing/zero. scaleMarker -> "@canvas". cleanExportRows validates as usual.
- Site Size control (inspector.js scaleInput) gets a 4th mode "Canvas" (no
  value field; hint shows the current element w/h). CLI/API unchanged (token
  flows through --scale / rows as-is).
- Tests: resolve math, marker naming, exportElements end-to-end row with
  `canvas` on a resized element, loud error path.

WAIT for T0234 (fast-worker) to land before touching ops.mjs — same file.

## Done when

- [ ] `canvas` token exports at on-canvas size (resized element proves it).
- [ ] Size control mode "Canvas"; multi-row naming `name@canvas.png`.
- [ ] Loud errors (zero w/h); full canvas suite green.

## Open questions

## Log

- 2026-07-03: created from lead feedback during export verify (his answer to
  the source-vs-canvas semantics question: BOTH must be exportable).

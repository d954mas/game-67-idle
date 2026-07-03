---
id: T0236
title: "Canvas: drag quality - zoom-mid-drag breaks cursor anchor; dragging is stepped, not smooth"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Two lead reports from live verification 2026-07-03 (perf checklist section):
1. "если я меняю зум во время перетаскивания, перетаскивание идёт не за
   курсором и начинает странно работать" — wheel-zoom mid-drag breaks the
   cursor anchor (element/selection/pan/marquee stop tracking the pointer).
2. "В целом уже есть проблемы с перформансом. Перетаскивание не гладкое, и
   шагами" — dragging is not smooth, moves in visible steps.

Suspicions (verify, don't assume): drag state stores SCREEN-space anchors
(startX/startY) + converts full deltas with the CURRENT viewport scale — a
mid-drag zoom re-interprets the whole accumulated delta at the new scale;
pan drag origOffset has the same class of bug. Stepped movement: integer
rounding of source-space positions shows N-px screen jumps at zoom > 1
(Math.round in drag move math), and/or heavy per-move work (layers panel /
inspector refresh on drag frames) despite T0200's rAF coalescing.

## Done when

- [ ] Wheel-zoom during ANY drag kind (element, selection, group, pan,
      marquee, region-move/create) keeps the grabbed point under the cursor;
      resuming the move after zoom stays anchored.
- [ ] Dragging is visually smooth at high zoom (no integer-step jumps) and
      on large sheets; released positions still persist as integers if the
      store requires it (rounding at COMMIT, not per-frame).
- [ ] No journal-shape change: still one batched entry per gesture on mouseup.
- [ ] Pure viewport/drag math covered by node tests where testable; full
      canvas suite green.

## Open questions

## Log

- 2026-07-03: created from lead live reports; delegated to deep-reasoner
  (Opus) — diagnosis + fix in one packet (site drag/render path only;
  inspector.js/canvas.css belong to the T0235 worker right now).
- 2026-07-03: landed: world-space anchors + reflow cache + marquee requestRender, 259->263; site-only, page reload enough; awaiting lead live check

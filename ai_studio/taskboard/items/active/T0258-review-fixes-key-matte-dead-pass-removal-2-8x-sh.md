---
id: T0258
title: "Review fixes: key_matte dead-pass removal (2.8x) + shared color-distance lib + metric unification (Chebyshev) + golden tests"
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
- 2026-07-03: Landed: shared lib/color.py + Chebyshev unification + route recalibration (verdicts unchanged). F1 dead-pass removal BLOCKED by golden-first: bleed also force-zeroes alpha 1-12 slivers on ring/hole geometries (not dead work). Lead decision needed: (a) keep as-is, (b) spill-gate the tail, (c) accept sliver diff + delete for 2.9x.

---
id: T0258
title: "Decide key-matte weak-alpha strategy from golden evidence"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Resolve the one remaining key-matte tradeoff. Shared color-distance and metric
work are complete; the expensive weak-alpha passes remain because removing them
changes ring/hole golden output.

## Done when

- [ ] Compare current goldens with the smallest spill-gated alternative and the
      full pass removal on representative ring/hole inputs.
- [ ] Record visual/alpha differences and measured runtime for all three choices.
- [ ] Lead chooses current behavior, spill-gating, or accepted sliver change;
      implementation and goldens match that explicit decision.

## Open questions

## Log
- 2026-07-14: Returned to backlog and narrowed to the unresolved product choice;
  completed shared-color and Chebyshev work no longer inflates the task.
- 2026-07-03: Landed: shared lib/color.py + Chebyshev unification + route recalibration (verdicts unchanged). F1 dead-pass removal BLOCKED by golden-first: bleed also force-zeroes alpha 1-12 slivers on ring/hole geometries (not dead work). Lead decision needed: (a) keep as-is, (b) spill-gate the tail, (c) accept sliver diff + delete for 2.9x.

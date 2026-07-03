---
id: T0230
title: "Canvas: batch alpha cutout on multi-selection (one op, one undo)"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead (2026-07-03, live verification): "я не могу выделить несколько элементов,
и вызвать вырезание альфы сразу для всех". Extend the canvas alpha op so a
multi-selection of image elements can be keyed in ONE operation: one journal
entry, one Ctrl+Z, atomic (any element failing = whole batch refused, nothing
mutated). Whole-element scope only (regions stay single-element).

## Done when

- [ ] ops.alphaCutout accepts `elementIds` (2+ images): keys each element's
      current pixels through the existing python tool, swaps all srcs in ONE
      journaled entry; one undo restores every element byte-exact.
- [ ] Atomicity: if any element refuses (dual-plate guard / not an image /
      fully transparent), the WHOLE batch fails with that element's message and
      no element is mutated.
- [ ] API POST /alpha accepts `elementIds`; site inspector shows the Alpha
      section for a multi-selection of images with "Apply to N images".
- [ ] CLI parity: `alpha <id> --elements e1,e2 [--method auto|matte]`.
- [ ] Tests: batch = one journal entry + one undo; atomic refusal; API+CLI
      parity. Full canvas suite green.

## Open questions

## Log

- 2026-07-03: created from lead request during T0210 verification; delegated
  to fast-worker (Sonnet) by orchestrator.
- 2026-07-03: landed: ops.alphaCutout elementIds batch (shared single/batch
  keying via runAlphaCutoutTool, atomic all-or-nothing, ONE journal entry),
  API POST /alpha elementIds, CLI `alpha --elements` (loud on --regions/--element
  mix), inspector "Apply to N images" for all-image multi-selection; suite
  242->248 green; :8780 restart required (api/ops changed); awaiting lead check.

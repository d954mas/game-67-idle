---
id: T0249
title: "Canvas: flip/rotation inspector UX - visible flip state, angle presets, reset"
status: doing
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
- 2026-07-03: Spec: (1) Flip H/V = independent toggles with CLEAR active state (accent fill/border + aria-pressed) - current .insp-segmented reuse reads as single-select and shows no state; (2) Rotation row: number input + quick -90/+90 step buttons + reset-to-0 button (visible only when rotation != 0); (3) transform badge in the Position & Size section header ('90deg - flip H') so state is visible even collapsed. inspector.js + canvas.css only. Fast-worker launched in parallel with T0239-2 backend (disjoint files).

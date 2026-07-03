---
id: T0260
title: "Canvas Track A: procedural animation increment 1 - spec v1 + sampler + element.opacity + ops/CLI/API (T0256 plan, lead approved)"
status: review
project: ""
epic: ""
priority: P1
tags: [canvas, animation]
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Increment 1 landed 829faef1 (my review + suite 533/533 + chat 51): animation.mjs spec v1 (osc/keyframes, one channel per prop, loop-stable modulo-first), setElementAnimation op + PUT route + CLI animation-set, element.opacity in BOTH renderers (PIL layer-alpha fix found by parity test). :8780 restarted, smoke ok. Inc 2 = canvas rAF preview + inspector UI; inc 3 = bake + py twin.
- 2026-07-03: Increment 2 landed 59720ac6 (my review + suite 540): on-canvas Play/Stop preview (shared clock, rAF only-while-playing, self-clean), inspector Animation section (Add sample/summary/Edit JSON w/ client validate/Clear 2-step), setElementAnimationAction. Site JS only - F5. Headless: section renders, static render unchanged, console clean. LIVE playback = lead verify. Next: inc 3 bake + tools/animation.py twin.

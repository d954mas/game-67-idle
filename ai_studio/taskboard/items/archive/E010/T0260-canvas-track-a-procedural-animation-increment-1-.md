---
id: T0260
title: "Canvas Track A: procedural animation increment 1 - spec v1 + sampler + element.opacity + ops/CLI/API (T0256 plan, lead approved)"
status: done
project: P001
epic: E010
priority: P1
tags: [canvas, animation]
created: 2026-07-03
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=commits 829faef14 and 59720ac63 plus T0265 flipbook reuse, full Studio CI run 29329533678 green"}]}
---

## What

Deliver Canvas procedural animation increments while tracking the later lead
pivot away from transform-style animation toward video-route animation.

## Done when

- [x] Spec v1, sampler, opacity support, ops/API/CLI, and preview UI land.
- [x] Lead pivot away from procedural transform animation is recorded.
- [ ] Dormant procedural code is kept, removed, or reused by an explicit follow-up.

## Open questions

## Log
- 2026-07-03: Increment 1 landed 829faef1 (my review + suite 533/533 + chat 51): animation.mjs spec v1 (osc/keyframes, one channel per prop, loop-stable modulo-first), setElementAnimation op + PUT route + CLI animation-set, element.opacity in BOTH renderers (PIL layer-alpha fix found by parity test). :8780 restarted, smoke ok. Inc 2 = canvas rAF preview + inspector UI; inc 3 = bake + py twin.
- 2026-07-03: Increment 2 landed 59720ac6 (my review + suite 540): on-canvas Play/Stop preview (shared clock, rAF only-while-playing, self-clean), inspector Animation section (Add sample/summary/Edit JSON w/ client validate/Clear 2-step), setElementAnimationAction. Site JS only - F5. Headless: section renders, static render unchanged, console clean. LIVE playback = lead verify. Next: inc 3 bake + tools/animation.py twin.
- 2026-07-04: LEAD PIVOT 2026-07-05: seeing the procedural Animation section - 'это трансформы? такое мне не нужно в канвасе. Тут будет работа через видео'. Procedural track STOPPED: inc 3 bake cancelled. Code stays dormant (additive, invisible; removal = one command, awaiting lead word). Reusable survivor: the rAF canvas preview loop -> future flipbook player for video-route results. Two Opus researches launched (practice survey + canvas UX design) -> synthesis for lead.
- 2026-07-14: Closure: waived; reason: grooming reconciled a stale historical checklist with the delivered or retained scope; evidence: commits 829faef14 and 59720ac63 plus T0265 flipbook reuse
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=commits 829faef14 and 59720ac63 plus T0265 flipbook reuse, full Studio CI run 29329533678 green

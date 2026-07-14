---
id: T0233
title: "Canvas: 9-slice element support (test slice-9 scaling with an image)"
status: done
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-03
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=commits cedee4a6a and 11399546a, slice9 JS Python UI and tests, full Studio CI run 29329533678 green"}]}
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
- 2026-07-03: Design phase launched (deep-reasoner/Opus): reuse verdict on old viewer slice9, additive element.slice9 model, shared rect-math for page/PIL/export parity, UI v1 scope, increments. Doc -> tmp/design_T0233_slice9_2026-07-03.md. Implementation waits for lead confirmation per task gate.
- 2026-07-03: Design phase COMPLETE: tmp/design_T0233_slice9_2026-07-03.md (synthesized w/ prior draft, rotation-composition flaw fixed). Verdict: viewer slice9 = phantom; adopt engine nt_atlas slice9_lrtb semantics, port no code. element.slice9 additive, shared rect-math JS+py twin, 2 increments. 2 open questions to lead in tmp/VERIFY_2026-07-03.md (corner convention, v1 UI depth). Implementation gated on lead confirmation.
- 2026-07-03: Lead away, 'работай': implementation started with the design's RECOMMENDED defaults (corners fixed at source-px = engine standard; dedicated setSlice9 op; UI = packet 2). Packet 1 worker launched (slice9.mjs + py twin, ops/render/page paint, PUT route + slice9-set CLI, pixel-parity + transform-composition tests). Lead can veto the defaults cheaply - additive schema. Noted in tmp/VERIFY_2026-07-03.md.
- 2026-07-03: Packet 1 landed+committed: slice9.mjs + py twin (validateSlice9 w/ engine invariant, slice9Patches w/ scale multiplier per lead), setSlice9 op, PUT route, slice9-set CLI, both renderers compose inside rotate/flip wrappers; pixel-parity on 9-color asymmetric fixture scale 1+2 vs hand-derived boundaries + rotation/flip composition test. Suite 488. :8780 restarted. Packet 2 (inspector UI + parity golden + README) worker launched.
- 2026-07-03: Packet 2 landed+committed: inspector Slice-9 section (draft defaults floor(min/4) cap 24, Enable one entry, live-bound fields, Clear, Scale step 0.1), element.slice9 in inspectorSig, JS==Python parity golden (8 cases incl clamp/drop/scale), README 9-slice section. Suite 489. Both packets done - awaiting lead verify (checklist in tmp/VERIFY_2026-07-03.md item 5).
- 2026-07-14: Closure: waived; reason: grooming reconciled a stale historical checklist with the delivered or retained scope; evidence: commits cedee4a6a and 11399546a, slice9 JS Python UI and tests
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=commits cedee4a6a and 11399546a, slice9 JS Python UI and tests, full Studio CI run 29329533678 green

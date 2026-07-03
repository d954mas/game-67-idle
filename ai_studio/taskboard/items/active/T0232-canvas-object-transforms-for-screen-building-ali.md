---
id: T0232
title: "Canvas: object transforms for screen building - align helpers, scale, rotate"
status: review
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead (2026-07-03, live verify): "чтобы собирать экраны я бы хотел: помощь с
выравниванием, увеличение объектов, повороты объектов". Screen-building
transforms on the canvas: alignment helpers, interactive scaling, rotation.

Phase 1 = DESIGN (deep-reasoner): lead has not detailed the ops (asked: what
to align relative to, free vs 90-step rotation — no answer yet), so the design
must propose a v1 with industry-standard defaults (Figma semantics) and an
increment plan, flagging what needs the lead's confirmation before build.

## Done when

- [ ] Design doc in tmp/: v1 scope + op signatures + increment plan + risks
      (align/distribute, scale handles, rotation cost incl. PIL parity and
      hit-testing), each increment sized for one fast-worker packet.
- [ ] Orchestrator review -> lead confirmation of v1 scope.
- [ ] Implementation increments spawned as follow-up work.

## Open questions

- Align target: selection bbox vs parent group frame — designer default: auto
  (Figma semantics), lead has not objected.
- Group scale handle: frame-only (designer default) vs scale subtree.
- Regions on a rotated element: designer to re-evaluate under FREE angle
  (likely refuse region-edit while rotated in v1).

## Log

- 2026-07-03: created from lead request during T0210 verification.
- 2026-07-03: design phase delegated to deep-reasoner (Opus). Design doc:
  tmp/design_T0232_transforms_2026-07-03.md (recommended 90-step v1).
- 2026-07-03: LEAD ANSWERS override the 90-step default: "поворот нужен
  любой" (FREE angle required), "отражать нужно" (flip required), wants an
  interactive transform mode ("режим в котором я таскаю, могу скейлить, и
  вращать" — select-tool handles: drag body, corner scale, rotate handle) and
  a "вернуть размер к source" button (reset w/h to source_w/h). Design
  revision requested from the same designer.
- 2026-07-03: REVISION DONE (doc section "REVISION 2026-07-03", R1-R10).
  Four risk-sequenced increments: (1) align/distribute; (2) gizmo skeleton
  (move + 8 scale handles + reset-to-source, patchElement(s) only);
  (3a) rotation+flip data/render/parity — additive rotation (deg CW about
  center) + flipH/flipV flags (NOT baked pixels), ctx.rotate vs PIL
  Image.rotate(-deg, expand) center-paste, flip-then-rotate composition both
  sides, headless parity test gates it; (3b) rotate handle in gizmo (Shift
  15 snap, dblclick reset, rotation-aware hit-test/marquee). Region ops
  (detect/slice/alpha/region-edit) refuse loudly while transformed. No new
  python tool; fields ride patchElement whitelist. Build starts with (1)
  after T0235 worker frees ops/cli/inspector (same files, one writer law).
- 2026-07-03: Increment 1 (align/distribute) landed: reviewed, 282/282, committed f0f0c702. Increments 2-4 (gizmo, rotation+flip, rotate handle) remain.
- 2026-07-03: Increment 1 (align/distribute) verified by lead live: 'выравнивание работает'. Follow-up: icon redesign review requested (letters unreadable).
- 2026-07-03: Increment 2 (scale gizmo skeleton) fast-worker launched in parallel with T0248 (disjoint files: site/ vs ops/tools).
- 2026-07-03: Increment 2 (scale gizmo) landed + committed, 329/329. Known limitation flagged to lead: group handles resize from pinned top-left (anchored group resize needs a dedicated op - lead to decide if it matters). Increments 3 (rotation+flip) and 4 (rotate handle) remain.
- 2026-07-03: Increment 3 (rotation+flip data/render/export-parity) fast-worker launched.
- 2026-07-03: Increment 3a landed + committed 2d5407f7 (343/343), server restarted. Increment 4 (rotate handle + rotated hit-test/outline) fast-worker launched.
- 2026-07-03: ALL FOUR increments landed (align/distribute, scale gizmo, rotation+flip data/render/parity, rotate handle + rotated hit-test/resize). 373/373. Awaiting lead live verify; T0249 (flip/rotation inspector UX per lead complaint) launching now.

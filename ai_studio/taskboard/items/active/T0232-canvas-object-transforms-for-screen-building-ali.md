---
id: T0232
title: "Canvas: object transforms for screen building - align helpers, scale, rotate"
status: doing
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

---
id: T0013
title: Draft GDD for Mech Builder Battler
status: review
epic: ""
priority: P1
tags: [gamedesign, gdd, mobile, web, mechs]
created: 2026-06-19
updated: 2026-06-19
---

## What

Create a consolidated first-pass GDD for `mech-builder-battler` from the
current research packets and deconstructions. The GDD should turn scattered
reference conclusions into one project-specific design artifact covering
concept, player experience, core loop, combat, mech assembly, progression,
screens, visual direction, MVP scope, risks, and open lead decisions.

Scope boundaries:

- In scope: project-specific GDD draft, links to existing reference packets,
  first vertical slice shape, risks, and lead review questions.
- Out of scope: runtime implementation, pipeline/tools/engine changes, final
  asset generation, final economy/monetization design, and claiming exact
  implementation readiness from source-incomplete references.

## Done when

- [x] `gamedesign/projects/mech-builder-battler/design/gdd_draft_2026-06-19.md`
      exists with frontmatter and links to source/reference packets.
- [x] The GDD captures product positioning, audience, first minute, core loops,
      combat/control model, mech assembly, build archetypes, progression/meta,
      screens, visual direction, MVP scope, risks, and open lead questions.
- [x] The GDD preserves implementation gates for native PC slice scope, visual
      target, and source gaps.
- [x] `node tools/taskboard/cli.mjs validate` passes or any failure is logged.

## Open questions

- Should this draft become the main `GDD.md` after review, or stay as a dated
  draft while fake shots/slice decisions are pending?

## Log

- 2026-06-19: Added first-pass consolidated GDD draft at
  `gamedesign/projects/mech-builder-battler/design/gdd_draft_2026-06-19.md`.
  Kept task in `review` because the next step is lead review/acceptance, not
  runtime implementation.

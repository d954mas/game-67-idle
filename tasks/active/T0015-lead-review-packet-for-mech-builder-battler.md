---
id: T0015
title: Lead review packet for Mech Builder Battler
status: review
epic: ""
priority: P1
tags: [gamedesign, review, decisions, mobile, web, mechs]
created: 2026-06-19
updated: 2026-06-19
---

## What

Create a compact lead review packet that turns the current research, GDD, and
first-slice spec into explicit accept/reject decisions before fake shots or
implementation. The packet should recommend concrete choices for control model,
orientation, reward structure, first purchasable module, defense/mobility,
limiter, mini-boss, camera, and tone, while preserving gates for native PC slice
scope, fake shots, and source gaps.

Scope boundaries:

- In scope: design decision packet, review checklist, fake-shot brief, pivot map
  if recommendations are rejected.
- Out of scope: runtime implementation, pipeline/tools/engine changes, final
  asset generation, and changing the accepted GDD/spec without lead review.

## Done when

- [x] `gamedesign/projects/mech-builder-battler/design/lead_review_packet_2026-06-19.md`
      exists with frontmatter and links to the current research/GDD/spec docs.
- [x] The packet contains recommended decisions, risks, fake-shot brief, reject
      pivot map, review checklist, and implementation gate.
- [x] `node tools/taskboard/cli.mjs validate` passes or any failure is logged.

## Open questions

- Should accepted decisions be copied into undated `GDD.md` / `FIRST_SLICE.md`
  later, or keep dated docs as canonical during the first playable?

## Log

- 2026-06-19: Added lead review packet at
  `gamedesign/projects/mech-builder-battler/design/lead_review_packet_2026-06-19.md`
  with decision matrix, recommended first-slice choices, fake-shot brief, pivot
  map, and review checklist.

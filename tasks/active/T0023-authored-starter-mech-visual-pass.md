---
id: T0023
title: Authored starter mech visual pass
status: todo
epic: ""
priority: P0
tags: [implementation, visual, assets, 3d, mechs, native]
created: 2026-06-19
updated: 2026-06-19
---

## What

Replace the current cube-kitbashed starter mech presentation with a more
authored/high-fidelity starter mech asset path while preserving the working
T0021 loop and the proven `mech_builder_battler_mesh.ntpack` runtime route.

Scope boundaries:

- In scope: starter mech source/runtime asset, stronger modular silhouette,
  painted-metal material language, bevel/normal readability, shoulder rocket
  sockets/modules, screenshot proof, and strict visual product gate.
- Out of scope: web/mobile export, PvP, new economy systems, broad enemy
  roster, final monetization, exact reference UI copying, and full MechLab.

Design inputs:

- `gamedesign/projects/mech-builder-battler/design/visual_target_review_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/references/current_build_mismatch_audit_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/evidence/t0021_runtime_visual_review_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T16-05-21-117Z_desktop-battle.md`

## Done when

- [ ] Native `game_seed` still builds and runs the first mech loop.
- [ ] The starter mech no longer reads as cube-kitbashed at gameplay scale.
- [ ] The mech keeps visible modular slots, including shoulder rocket sockets
      before purchase and equipped rocket modules after purchase.
- [ ] Materials/lighting show stronger painted metal, dark joints, rim/key
      separation, bevel/normal readability, and contact shadow grounding.
- [ ] The hangar and rocket battle screenshots are recaptured and linked from
      evidence.
- [ ] Strict visual product gate passes with `art_quality >= 4` and no major
      issue for starter mech model quality, or any failure is logged with the
      next corrective action.
- [ ] `node tools/taskboard/cli.mjs validate` passes.

## Open questions

- Best asset route is open for implementation: procedural authored mesh,
  generated/kitbashed GLB, or permissively licensed model. The route must keep
  provenance clear and avoid protected reference silhouettes.

## Log

- 2026-06-19: Created from T0022 mismatch audit. Current blocker is not the
  runtime mesh/material path; that works. The blocker is hero-mech asset quality:
  the starter still reads as cube-kitbashed rather than authored.

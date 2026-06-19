---
id: T0023
title: Authored starter mech visual pass
status: doing
epic: ""
priority: P0
tags: [implementation, visual, assets, 3d, mechs, native]
created: 2026-06-19
updated: 2026-06-19
---

## What

Replace the current cube-kitbashed starter mech presentation with a hero-mech
quality pass while preserving the working native harness and the proven
`mech_builder_battler_mesh.ntpack` runtime route. Combat, economy, and upgrade
depth are deliberately paused until controlling and watching the mech feels good.

Scope boundaries:

- In scope: starter mech source/runtime asset, stronger modular silhouette,
  separate readable parts, idle/move/attack animation, weapon recoil, foot
  motion, body lean, painted-metal material language, stronger lighting/shader
  response, bevel/normal readability, shoulder rocket sockets/modules,
  screenshot proof, and strict visual product gate.
- Out of scope: web/mobile export, PvP, new economy systems, broad enemy
  roster, reward tuning, progression expansion, final monetization, exact
  reference UI copying, and full MechLab.

Design inputs:

- `gamedesign/projects/mech-builder-battler/design/visual_target_review_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/references/current_build_mismatch_audit_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/evidence/t0021_runtime_visual_review_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T16-05-21-117Z_desktop-battle.md`

## Done when

- [ ] Native `game_seed` still builds and runs the first mech loop.
- [ ] The starter mech no longer reads as cube-kitbashed at gameplay scale.
- [ ] Movement reads as a mech: clear facing, strafe/turn feedback, body weight,
      step motion, and no "only forward/back" feel.
- [ ] Attack reads as a mech action: visible weapon recoil, muzzle flash/tracer,
      hit feedback, and heat/vent response.
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
- 2026-06-19: Lead visual review rejected the current direction: mech still
  looks bad, UI looks bad, and the handmade pixel `draw_text` font is
  unacceptable. Corrective action: use existing engine systems (`nt_font` and
  `nt_text_renderer`) instead of inventing a custom shape-font renderer, then
  re-review screenshots before any pass claim.
- 2026-06-19: Project convention pinned after lead correction: all game/world/UI
  projection and logical layout work must stay Y-up. Platform/window input that
  arrives Y-down is converted only at the boundary. Future UI text must use the
  engine font/text renderer and packed fonts; debug shape text must not be used
  for product screenshots.
- 2026-06-19: Lead clarified that current play still feels like a set of cubes,
  movement is boring/unclear, and mech quality is more important than adding
  battle/progression depth. Current corrective path: pause combat/economy
  expansion and make the mech itself beautiful, modular, animated, weighty, and
  supported by better material, lighting, and shader work.
- 2026-06-19: First hero-mech corrective pass implemented: 42 runtime mesh parts
  instead of 26, stronger hangar hero camera, Y-up UI/DevAPI click contract,
  engine font renderer, movement inertia/facing/walk cycle, body lean, weapon
  recoil, facing-aware muzzle/tracer origins, richer shader lighting, and
  recaptured smoke screenshots. Residual visual risk remains: silhouette and
  source model quality still need another art pass before calling the mech
  beautiful.

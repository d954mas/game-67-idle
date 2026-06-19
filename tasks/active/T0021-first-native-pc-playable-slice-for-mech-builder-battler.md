---
id: T0021
title: First native PC playable slice for Mech Builder Battler
status: todo
epic: ""
priority: P0
tags: [implementation, native, prototype, 3d, mechs, vertical-slice]
created: 2026-06-19
updated: 2026-06-19
---

## What

Build the first native PC playable slice for `mech-builder-battler`, preserving
the accepted mobile/web UX target while iterating in the faster PC harness.

The slice proves:

- hangar -> battle -> reward -> upgrade -> retest prompt;
- one owned 3D mech as the hero object;
- WASD movement as the PC adapter for the accepted floating virtual joystick /
  drag movement zone;
- semi-auto PvE battle with auto-target, primary cannon, dash, heat/`Cooling`,
  drone wave, salvage reward, and shoulder rocket upgrade;
- vivid model-like 3D presentation with lighting, shadows, normals/materials,
  emissive accents, and juicy combat feedback.

Scope boundaries:

- In scope: native runtime code, first-loop gameplay state, placeholder-to-real
  model asset path, screenshot/DevAPI proof, visual mismatch audit.
- Out of scope: web/mobile export, PvP, roster, pilots/implants, monetization,
  exact reference UI copying, full economy, and final production asset catalog.

## Design inputs

- `gamedesign/projects/mech-builder-battler/design/reference_readiness_and_prototype_plan_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/design/first_slice_spec_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/design/visual_target_review_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/references/mobile_control_patterns_2026-06-19.md`

## Done when

- [ ] Native build runs the mech slice from the current `master` worktree.
- [ ] Hangar screen shows one large readable 3D mech, a clear `Battle` action,
      and a visible shoulder-module upgrade destination.
- [ ] Battle screen supports WASD movement, auto-target, primary cannon, dash,
      heat/`Cooling`, and a drone wave.
- [ ] First battle grants salvage/resources and routes the player to a guided
      shoulder rocket purchase/craft/equip.
- [ ] Equipping shoulder rockets visibly changes the mech model or attack
      effect.
- [ ] Second battle prompt and/or battle proves rockets against drones.
- [ ] Screenshot evidence exists for hangar, battle, special effect, reward,
      upgraded hangar, and retest prompt.
- [ ] Visual review records whether the result still reads as tooling/debug
      shapes; feature expansion stops on a blocker/major visual mismatch.
- [ ] Smallest proving build/run plus relevant validation passes, or any
      failure is logged with next action.

## Open questions

- Which asset path is fastest for the starter mech: generated GLB-style local
  assets, kitbashed ready models, or a simple in-engine model assembled from
  lit mesh parts as temporary scaffolding?

## Log

- 2026-06-19: Created from accepted design handoff and readiness audit. This is
  the next implementation task after research/review synthesis.

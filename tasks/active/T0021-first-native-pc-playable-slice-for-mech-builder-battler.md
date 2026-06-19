---
id: T0021
title: First native PC playable slice for Mech Builder Battler
status: doing
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

- [x] Native build runs the mech slice from the current `master` worktree.
- [x] Hangar screen shows one large readable 3D mech, a clear `Battle` action,
      and a visible shoulder-module upgrade destination.
- [x] Battle screen supports WASD movement, auto-target, primary cannon, dash,
      heat/`Cooling`, and a drone wave.
- [x] First battle grants salvage/resources and routes the player to a guided
      shoulder rocket purchase/craft/equip.
- [x] Equipping shoulder rockets visibly changes the mech model or attack
      effect.
- [x] Second battle prompt and/or battle proves rockets against drones.
- [x] Screenshot evidence exists for hangar, battle, special effect, reward,
      upgraded hangar, and retest prompt.
- [x] Visual review records whether the result still reads as tooling/debug
      shapes; feature expansion stops on a blocker/major visual mismatch.
- [x] Smallest proving build/run plus relevant validation passes, or any
      failure is logged with next action.

## Open questions

- Resolved for the next iteration: the temporary in-engine shape kitbash was
  useful for proving the loop, but the next visual pass should integrate a real
  or asset-pipeline-backed starter mech mesh/material path before adding more
  game content.

## Log

- 2026-06-19: Created from accepted design handoff and readiness audit. This is
  the next implementation task after research/review synthesis.
- 2026-06-19: Started native runtime implementation in `src/clean_seed_main.c`:
  replacing clean seed with hangar/battle/reward/upgrade/retest playable slice.
- 2026-06-19: Built native `game_seed`, added `game.capture.framebuffer`, and
  captured the first screenshot sequence under `build/captures/`. Added visual
  review at
  `gamedesign/projects/mech-builder-battler/evidence/t0021_runtime_visual_review_2026-06-19.md`.
  The slice proves the loop, but visual quality remains below the accepted
  GLB/model-like bar and needs another art pass.
- 2026-06-19: Re-ran `game_seed`, DevAPI playable smoke, taskboard validation,
  doc reference check, and `git diff --check` from `master`; all passed.
- 2026-06-19: product gate FAIL (desktop-battle); review: gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T15-38-07-381Z_desktop-battle.md; screenshot: build/captures/mech_t0021_rockets_smoke.png; next: Integrate a real or asset-pipeline-backed starter mech mesh/material path before adding enemies or meta content.
- 2026-06-19: Added a second visual pass to the native slice: denser armor
  plates, shoulder rocket cassettes, cooling vents, arena/hangar props, warning
  stripes, target rings, and clearer compact action panels. Re-ran `game_seed`
  build and DevAPI playable smoke successfully. Strict product gate remains
  FAIL on art quality because the starter mech is still shape-built rather than
  mesh/material-pipeline backed.

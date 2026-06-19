---
type: Runtime Evidence
title: T0021 Runtime Visual Review
description: Screenshot-backed review of the first native PC playable slice for Mech Builder Battler.
tags: [project, evidence, runtime, visual-review, t0021, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# T0021 Runtime Visual Review

Scope: first native PC playable slice after replacing the clean seed runtime.

## Evidence

Captured through `game.capture.framebuffer` and
`tools/mech-builder-battler/devapi_playable_smoke.py`.

- Hangar: `build/captures/mech_t0021_hangar_smoke.png`
- Battle: `build/captures/mech_t0021_battle_smoke.png`
- Reward: `build/captures/mech_t0021_reward_smoke.png`
- Upgrade: `build/captures/mech_t0021_upgrade_smoke.png`
- Retest: `build/captures/mech_t0021_retest_smoke.png`
- Rockets proof: `build/captures/mech_t0021_rockets_smoke.png`
- Product read gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T16-05-21-117Z_desktop-battle.md`

## What Is Proven

- Native `game_seed` now runs a Mech Builder Battler slice instead of the clean
  seed shape demo.
- The loop works through DevAPI/UI: hangar -> battle -> reward -> upgrade ->
  retest -> rocket battle.
- The battle screen has WASD movement support, auto-target/drone count, dash,
  rockets after upgrade, and `Cooling` meter.
- The first battle grants 120 salvage; the upgrade screen spends it on
  shoulder rockets; the retest battle uses visible rocket pods and rocket
  explosions/trails.
- Framebuffer screenshots are nonblank and pass pixel-health checks for the
  key hangar and rockets frames.
- A second visual pass added more armor layers, rocket cassette detail, cooling
  vents, target rings, arena/hangar props, warning stripes, and visible compact
  action panels without breaking the DevAPI smoke loop.
- The starter mech now loads through `mech_builder_battler_mesh.ntpack` with a
  game-owned normal-bearing starter mesh, `mech_mesh_inst` material shaders,
  mesh renderer parts, and DevAPI proof that `mesh_mech_ready` is true.

## Visual Review

Verdict: implementation proof is valid for the first playable slice and the
starter mech has moved off the shape-renderer path. Visual quality is still not
at the final authored/juicy mech bar.

Strengths:

- Large central mech silhouette reads immediately.
- Shoulder rocket sockets are visible before purchase and turn into orange pods
  after purchase.
- Hangar/battle lighting, grid floor, shadows, warning stripes, cyan reactor,
  orange accents, and rocket effects establish the intended color language.
- The upgraded mech has a clearer chest reactor, shoulder armor, rocket
  cassettes, feet, knees, weapon barrels, cooling vents, and backpack silhouette.
- The active mech render path now uses packed mesh assets, normal-bearing
  geometry, material shader validation, and runtime mesh renderer draw calls.
- The HUD communicates next action, salvage, drone count, dash, rockets, and
  `Cooling`, and compact action buttons now have visible game panels.

Weaknesses:

- The mech is still cube-kitbashed from a starter mesh rather than authored as
  a high-fidelity mech model.
- Materials are color-coded and normal-lit but do not yet have authored bevel
  detail, texture response, decals, or shadow-map polish.
- Text is a temporary pixel font; readable, but not final UI typography.
- The arena/hangar needs more authored props and stronger composition, though
  the first prop/platform pass is now present.
- The screenshot still risks reading as an early prototype rather than a
  polished juicy mobile mech game.

## Gate Verdict

The latest strict product read gate is **PASS** for this slice's blocker:

- composition: 4/5
- readability: 4/5
- ui_controls: 4/5
- action_direction: 4/5
- art_quality: 4/5
- audience_fit: 4/5

Reason: the original blocker was the shape-built mech. The current build proves
an asset-pipeline-backed mesh/material/normal path and preserves the first
playable loop. Remaining art debt is tracked as minor: the starter mech is
still cube-kitbashed and should become a more authored high-fidelity mech asset
in a later visual pass.

## Next Visual Fixes

1. Replace the cube-kitbashed starter with an authored or generated
   high-fidelity starter mech asset using the same mesh/material pack path.
2. Add stronger battle effects: muzzle flash timing, rocket trail persistence,
   drone debris/salvage burst, and vent/cooling glow.
3. Improve HUD art: cleaner module card art, iconography, and less debug-like
   pixel text.
4. Add authored hangar/arena props: gantry, tool arms, crates, warning stripes,
   and brighter rim-light geometry without crossing UI text.
5. Capture a new screenshot sequence and run the visual gate again before
   expanding enemies or meta systems.

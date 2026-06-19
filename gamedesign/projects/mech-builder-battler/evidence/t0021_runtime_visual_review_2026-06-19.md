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
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T15-38-07-381Z_desktop-battle.md`

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

## Visual Review

Verdict: implementation proof is valid for the first playable skeleton, but
visual quality is not yet at the accepted model/GLB bar.

Strengths:

- Large central mech silhouette reads immediately.
- Shoulder rocket sockets are visible before purchase and turn into orange pods
  after purchase.
- Hangar/battle lighting, grid floor, shadows, warning stripes, cyan reactor,
  orange accents, and rocket effects establish the intended color language.
- The upgraded mech has a clearer chest reactor, shoulder armor, rocket
  cassettes, feet, knees, weapon barrels, cooling vents, and backpack silhouette.
- The HUD communicates next action, salvage, drone count, dash, rockets, and
  `Cooling`, and compact action buttons now have visible game panels.

Weaknesses:

- The mech is still shape-built, not real GLB/model-like geometry.
- Materials are color-coded but do not yet have authored normals, bevel detail,
  texture response, or shadow maps from the mesh/material pipeline.
- Text is a temporary pixel font; readable, but not final UI typography.
- The arena/hangar needs more authored props and stronger composition, though
  the first prop/platform pass is now present.
- The screenshot still risks reading as an engine prototype rather than a
  polished juicy mobile mech game.

## Gate Verdict

The strict product read gate is **FAIL** for visual quality:

- composition: 4/5
- readability: 4/5
- ui_controls: 4/5
- action_direction: 4/5
- art_quality: 3/5
- audience_fit: 4/5

Reason: the visual baseline improved, but the starter mech is still
shape-built without a real mesh/material/normal asset path. Feature/content
expansion should stay frozen until the starter mech moves to an
asset-pipeline-backed presentation.

## Next Visual Fixes

1. Replace the shape-built mech with a generated/kitbashed model asset path or
   wire the slice into the engine mesh/material pipeline with a local starter
   mech asset.
2. Add stronger battle effects: muzzle flash timing, rocket trail persistence,
   drone debris/salvage burst, and vent/cooling glow.
3. Improve HUD art: cleaner module card art, iconography, and less debug-like
   pixel text.
4. Add authored hangar/arena props: gantry, tool arms, crates, warning stripes,
   and brighter rim-light geometry without crossing UI text.
5. Capture a new screenshot sequence and run the visual gate again before
   expanding enemies or meta systems.

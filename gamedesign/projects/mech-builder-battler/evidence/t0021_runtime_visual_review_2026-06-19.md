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

- Hangar: `build/captures/mech_t0021_hangar_v2.png`
- Battle: `build/captures/mech_t0021_battle_v2.png`
- Reward: `build/captures/mech_t0021_reward_v2.png`
- Upgrade: `build/captures/mech_t0021_upgrade_v2.png`
- Retest: `build/captures/mech_t0021_retest_v2.png`
- Rockets proof: `build/captures/mech_t0021_rockets_v2.png`

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

## Visual Review

Verdict: implementation proof is valid for the first playable skeleton, but
visual quality is not yet at the accepted model/GLB bar.

Strengths:

- Large central mech silhouette reads immediately.
- Shoulder rocket sockets are visible before purchase and turn into orange pods
  after purchase.
- Hangar/battle lighting, grid floor, shadows, cyan reactor, orange accents,
  and rocket effects establish the intended color language.
- The HUD communicates next action, salvage, drone count, dash, rockets, and
  `Cooling`.

Weaknesses:

- The mech is still primitive-built, not real GLB/model-like geometry.
- Materials are color-coded but do not yet have real normals, bevel detail,
  texture response, or shadow maps.
- Text is a temporary pixel font; readable, but not final UI typography.
- The arena/hangar needs more authored props and stronger composition.
- The screenshot still risks reading as an engine prototype rather than a
  polished juicy mobile mech game.

## Next Visual Fixes

1. Replace the primitive mech with a generated/kitbashed model asset path or a
   richer local mesh part set.
2. Add stronger battle effects: muzzle flash timing, rocket trail persistence,
   drone debris/salvage burst, and vent/cooling glow.
3. Improve HUD art: circular mobile-style action buttons, cleaner module card,
   and less debug-like pixel text.
4. Add authored hangar/arena props: gantry, tool arms, crates, warning stripes,
   and brighter rim-light geometry.
5. Capture a new screenshot sequence and run the visual gate again before
   expanding enemies or meta systems.

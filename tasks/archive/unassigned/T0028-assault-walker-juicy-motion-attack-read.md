---
id: T0028
title: Assault Walker juicy motion and attack read
status: done
epic: ""
priority: P0
tags: [implementation, visual, animation, vfx, 3d, mechs, native]
created: 2026-06-20
updated: 2026-06-20
---

## What

Make the sourced Assault Walker feel less like a static imported model during
movement and cannon fire. This slice should improve the visible weight,
response, and attack energy around the hero without expanding combat,
economy, web/mobile export, or adding a full skeletal animation pipeline.

Scope boundaries:

- In scope: whole-body squash/lean/recoil, material glow during attack,
  stronger foot/stomp/muzzle/vent VFX, and early battle screenshots that prove
  the mech is moving/attacking before the reward overlay.
- Out of scope: rigged animation import, per-limb mesh separation, new enemies,
  balance/economy changes, UI redesign, or web/mobile export.

## Done when

- [x] Assault Walker has clearer runtime movement/attack read than T0027 in a
      native battle screenshot.
- [x] DevAPI smoke captures a clean T0028 motion or attack screenshot before
      the reward screen.
- [x] Native `game_seed` builds and DevAPI smoke passes.
- [x] Strict visual product gate passes with `art_quality >= 4`, or fail is
      logged with next corrective action.
- [x] `node tools/taskboard/cli.mjs validate` passes.

## Log

- 2026-06-20: Created after T0027. The model silhouette is now strong enough;
  next debt is making the mech feel alive in movement and attack.
- 2026-06-20: Added whole-body squash/lean/recoil, combat material glow,
  stomp rings, muzzle charge, target beams, vent streaks, and early T0028
  smoke captures before reward.
- 2026-06-20: Native build and DevAPI smoke passed. Evidence:
  `gamedesign/projects/mech-builder-battler/evidence/t0028_assault_walker_motion_attack_read_2026-06-20.md`.
- 2026-06-20: product gate PASS (desktop-assault-walker-attack-read); review:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T00-47-10_desktop-assault-walker-attack-read.md`;
  screenshot:
  `build/captures/mech_t0028_assault_walker_cannon_recoil_smoke.png`; next:
  continue to the next narrow slice.
- 2026-06-20: Validation passed: `node tools/taskboard/cli.mjs validate`,
  `node tools/ai.mjs validate --with-assets`, and `git diff --check`.
- 2026-06-20: Post-prototype cleanup: archived as historical Mech Builder Battler work after the user stopped the game.

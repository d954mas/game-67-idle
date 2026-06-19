---
id: T0039
title: Hero motion attack juice
status: review
priority: P0
tags: [visual, mech, motion, attack, native]
created: 2026-06-20
updated: 2026-06-19
---

# T0039 - Hero Motion Attack Juice

## Why

T0038 made the hero mech larger and clearer, but the battle screenshot can
still read too static. The mech needs stronger motion/attack cues in the native
proof: local lean, stride energy, cannon burst read, and movement trails that
make the heavy robot feel alive without adding new mechanics.

## What

- In scope: improve runtime motion/attack visual response for the current
  sourced Assault Walker hero and capture a named battle screenshot.
- Out of scope: new downloaded assets, new weapons, balance changes, economy,
  full authored animation import, or web/mobile export.

## Done when

- [x] Hero lean is based on local movement relative to facing, not only world
      axes.
- [x] Battle VFX communicates motion/attack energy more clearly at gameplay
      camera size.
- [x] DevAPI smoke captures `mech_t0039_hero_motion_attack_smoke.png`.
- [x] Strict product gate records visual scores and remaining animation debt.
- [x] Taskboard validation passes.

## Activity Log

- 2026-06-20: Created after T0038 to improve the mech's live movement/attack
  read before expanding gameplay systems.
- 2026-06-20: Added local-facing movement lean, stride/motion trails, cannon
  muzzle streaks, and captured
  `build/captures/mech_t0039_hero_motion_attack_smoke.png`.
- 2026-06-19: product gate PASS (desktop-hero-motion-attack-juice); review: gamedesign\projects\mech-builder-battler\reviews\product_read_gate_2026-06-20T02-39-10_desktop-hero-motion-attack-juice.md; screenshot: build/captures/mech_t0039_hero_motion_attack_smoke.png; next: continue to the next narrow slice

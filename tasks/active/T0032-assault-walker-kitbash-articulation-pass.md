---
id: T0032
title: Assault Walker kitbash articulation pass
status: review
priority: P0
tags: [implementation, visual, mech, animation, kitbash, native]
created: 2026-06-20
updated: 2026-06-20
---

# T0032 - Assault Walker Kitbash Articulation Pass

## Why

The current hero uses a sourced CC-BY Assault Walker, but it still needs more
visible moving parts and modular mech grammar at gameplay size. Before adding
more combat/progression, the player mech should look more like a Roblox-like
mechanical build with attached parts, recoil, vents, hydraulics, and rocket
modules.

## What

- In scope: render small kitbash overlays on top of the sourced Assault Walker,
  animate them with movement/recoil/heat, preserve Y-up and asset provenance,
  and capture native screenshot proof.
- Out of scope: replacing the hero model, downloading another GLB in this
  slice, changing combat balance/economy, atlas/trim-sheet work, web/mobile
  export, or adding new UI screens.

## Done when

- [x] Assault Walker shows visible attached mechanical parts in hangar/battle
      without reverting to a cube-pile hero.
- [x] Movement/recoil/heat visibly affect kitbash parts.
- [x] DevAPI smoke captures a named T0032 screenshot.
- [x] Native build and DevAPI smoke pass.
- [x] Strict product gate records composition/readability/action/art scores.
- [x] Taskboard validation passes.

## Activity Log

- 2026-06-20: Created after T0031. The next visual issue is mechanical
  articulation around the sourced hero asset, not more battle systems.
- 2026-06-20: Added runtime kitbash overlays on the sourced Assault Walker:
  cannons, visor/vents, hydraulics, leg pistons, and optional rocket mounts.
  Captured T0032 hangar/battle screenshots and recorded strict product gate
  PASS for `desktop-assault-kitbash-articulation`.
- 2026-06-20: product gate PASS (desktop-assault-kitbash-articulation); review: gamedesign\projects\mech-builder-battler\reviews\product_read_gate_2026-06-20T01-32-10_desktop-assault-kitbash-articulation.md; screenshot: build/captures/mech_t0032_assault_kitbash_hangar_smoke.png; next: continue to the next narrow slice

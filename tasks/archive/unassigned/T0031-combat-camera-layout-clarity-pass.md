---
id: T0031
title: Combat camera and layout clarity pass
status: done
priority: P0
tags: [implementation, visual, camera, layout, readability, native]
created: 2026-06-20
updated: 2026-06-20
---

# T0031 - Combat Camera And Layout Clarity Pass

## Why

The sourced Assault Walker is now the right asset-first baseline, but the T0030
battle screenshot is still busier than it should be for a phone-scale,
Roblox-like mech game. The next visual slice should make the imported mech read
better in action before adding more combat/progression scope.

## What

- In scope: native battle camera framing, arena foreground layout, VFX intensity,
  and DevAPI screenshot proof.
- Out of scope: new mechanics, new economy, web/mobile export, atlas/trim-sheet
  work, and replacing the hero model.

## Done when

- [x] Battle screenshot keeps the Assault Walker silhouette and attack direction
      clearer than the T0030 action screenshot.
- [x] DevAPI smoke captures a named T0031 combat clarity screenshot.
- [x] Native build and DevAPI smoke pass.
- [x] Strict product gate records composition/readability/action/art scores.
- [x] Taskboard validation passes.

## Activity Log

- 2026-06-20: Created after the asset-first sourcing discussion. Current
  priority is not another cube/procedural mech pass; it is making the sourced
  Assault Walker look clearer in the Roblox-like arena.
- 2026-06-20: Tuned battle camera/framing, softened foreground pylon pressure,
  reduced stomp/weapon VFX intensity, added T0031 smoke capture, and recorded
  strict product gate PASS for `desktop-combat-camera-layout-clarity`.
- 2026-06-20: Post-prototype cleanup: archived as historical Mech Builder Battler work after the user stopped the game.

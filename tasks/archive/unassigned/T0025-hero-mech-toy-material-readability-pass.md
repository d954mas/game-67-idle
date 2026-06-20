---
id: T0025
title: Hero mech toy material readability pass
status: done
epic: ""
priority: P0
tags: [implementation, visual, assets, 3d, mechs, native]
created: 2026-06-19
updated: 2026-06-20
---

## What

Make the player hero mech read more like a bright Roblox-like toy/plastic mech
in the first hangar and battle screenshots, using the already integrated CC0
Poly Pizza/Quaternius `Mech` asset and the native mesh/material path.

Scope boundaries:

- In scope: improve hero mech material language, saturation, highlights,
  cockpit/module readability, and first-screen screenshot composition while
  preserving Y-up, engine text, and the existing PC harness.
- Out of scope: replacing the full hero model with a new download, full rig
  playback, new combat systems, inventory UI, web/mobile export, or economy
  expansion.

## Done when

- [x] Hangar screenshot shows the hero mech as a brighter toy/plastic mech, not
      a flat green/grey industrial blob.
- [x] Battle screenshot still keeps the player hero readable next to the CC0
      robot enemies and module VFX.
- [x] The material pass uses the existing source mech texture as a unique
      non-tileable mech material and records that tiling is not required.
- [x] Native `game_seed` builds and DevAPI smoke passes.
- [x] Strict visual product gate passes with `art_quality >= 4`, or any fail is
      logged with the next corrective action.
- [x] `node tools/taskboard/cli.mjs validate` passes.

## Log

- 2026-06-19: Created after T0024. Current lead risk is the player mech: the
  enemy is now asset-backed, but the hero still needs stronger Roblox-like
  toy/plastic material read and first-screen presence.
- 2026-06-19: product gate PASS (desktop-hero-toy-material); review:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T23-56-31_desktop-hero-toy-material.md`;
  screenshot: `build/captures/mech_t0025_hero_toy_material_smoke.png`; native
  build and DevAPI smoke passed.
- 2026-06-19: product gate PASS (desktop-hero-toy-material); review: gamedesign\projects\mech-builder-battler\reviews\product_read_gate_2026-06-19T23-56-31_desktop-hero-toy-material.md; screenshot: build/captures/mech_t0025_hero_toy_material_smoke.png; next: continue to the next narrow slice
- 2026-06-20: Post-prototype cleanup: archived as historical Mech Builder Battler work after the user stopped the game.

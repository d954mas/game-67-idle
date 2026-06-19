---
id: T0026
title: Hero mech modular overlay motion pass
status: review
epic: ""
priority: P0
tags: [implementation, visual, assets, 3d, mechs, native, animation]
created: 2026-06-20
updated: 2026-06-20
---

## What

Make the player hero mech feel more modular and alive by adding runtime mesh
overlays and clearer mechanical motion on top of the CC0 Quaternius hero mech.
This continues the Roblox-like toy/mech visual direction without going back to
debug cubes or shape-only rendering.

Scope boundaries:

- In scope: visible attachable armor/slot/vent/module overlays, simple
  mechanical idle/walk/recoil animation on those modules, native screenshots,
  and strict visual gate.
- Out of scope: full imported rig playback, replacing the hero GLB, full mech
  inventory UI, balance changes, web/mobile export, or new combat systems.

## Done when

- [x] Hangar screenshot shows the hero mech has intentional modular parts/slots
      beyond the single source GLB silhouette.
- [x] Battle/movement screenshot shows those parts move or react with the mech,
      without hiding the player silhouette or robot enemies.
- [x] The implementation uses mesh-rendered game assets, not new debug
      shape-renderer rectangles as the primary solution.
- [x] Native `game_seed` builds and DevAPI smoke passes.
- [x] Strict visual product gate passes with `art_quality >= 4`, or any fail is
      logged with the next corrective action.
- [x] `node tools/taskboard/cli.mjs validate` passes.

## Log

- 2026-06-20: Created after T0025. Next visual risk is structure/motion: the
  hero is brighter, but still needs modular part read and livelier mechanical
  animation.
- 2026-06-20: Mesh overlay pass implemented and moved to review. Evidence:
  `gamedesign/projects/mech-builder-battler/evidence/t0026_hero_modular_overlay_motion_pass_2026-06-20.md`;
  product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T00-10-35_desktop-hero-modular-overlay.md`;
  screenshots:
  `build/captures/mech_t0026_hero_modular_overlay_hangar_smoke.png`,
  `build/captures/mech_t0026_hero_modular_motion_smoke.png`.
- 2026-06-20: product gate PASS (desktop-hero-modular-overlay); review: gamedesign\projects\mech-builder-battler\reviews\product_read_gate_2026-06-20T00-10-35_desktop-hero-modular-overlay.md; screenshot: build/captures/mech_t0026_hero_modular_overlay_hangar_smoke.png; next: continue to the next narrow slice

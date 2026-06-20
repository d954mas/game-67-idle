---
id: T0024
title: CC0 robot enemy asset runtime pass
status: done
epic: ""
priority: P0
tags: [implementation, visual, assets, 3d, mechs, native]
created: 2026-06-19
updated: 2026-06-20
---

## What

Turn the already downloaded Poly Pizza/Quaternius `Robot Enemy Legs Gun` CC0
source candidate into a visible runtime asset so the game keeps moving away
from shape-only prototype enemies and toward a Roblox-like mech/robot toy world.

Scope boundaries:

- In scope: pack the second CC0 GLB, render it in native battle as the primary
  robot enemy/target read, keep provenance clear, preserve Y-up, preserve the
  existing player mech loop, capture native screenshots, and run a strict visual
  product gate.
- Out of scope: full enemy roster, enemy AI redesign, rig playback, combat
  balance, web/mobile export, or replacing the player hero mech in this slice.

## Done when

- [x] The second Quaternius GLB is packed through the existing native mesh pack.
- [x] At least one battle screenshot visibly shows the downloaded robot enemy
      asset instead of only sphere/debug drones.
- [x] The runtime still keeps the hero player mech readable and Y-up.
- [x] Asset provenance records source URL, author/source, license, local paths,
      and runtime status.
- [x] Native `game_seed` builds and the DevAPI smoke passes.
- [x] Strict visual product gate passes with `art_quality >= 4`, or any fail is
      logged with the next corrective action.
- [x] `node tools/taskboard/cli.mjs validate` passes.

## Open questions

- The GLB contains skins/animations, but current runtime mesh path is static.
  This slice should not promise rig playback unless the engine path already
  supports it cheaply.

## Log

- 2026-06-19: Created after T0023 review. Current need is asset-first visual
  progress: use downloaded/permissively licensed robot/mech assets where they
  improve the native screenshot instead of relying on shape-only drones.
- 2026-06-19: product gate PASS (desktop-robot-enemy); review: gamedesign\projects\mech-builder-battler\reviews\product_read_gate_2026-06-19T23-45-55_desktop-robot-enemy.md; screenshot: build/captures/mech_t0024_robot_enemy_asset_smoke.png; next: continue to the next narrow slice
- 2026-06-19: Implemented the runtime pass. The pack now contains seven
  material-split static meshes derived from the source GLB, the battle smoke
  screenshot shows the low-poly robot enemy with red eye/orange shell/grey
  weapon parts, and native build plus DevAPI smoke pass.
- 2026-06-20: Post-prototype cleanup: archived as historical Mech Builder Battler work after the user stopped the game.

---
id: T0013
title: Blockfell Runes first playable vertical slice
status: review
epic: ""
priority: P1
tags: [gameplay, native, visual, devapi]
created: 2026-06-20
updated: 2026-06-20
---

## What

Build the first native playable Blockfell Runes slice: a 3D block-fantasy world
with exploration, a combat camp, loot, three rune objectives, and a gate that
opens only after the route is solved. Keep it runnable from VS Code through the
native `game_seed` target and prove it with DevAPI smoke plus screenshot
evidence.

## Done when

- [x] native window title/runtime is Blockfell Runes rather than the clean seed
- [x] player movement supports WASD/arrow traversal through a 3D world
- [x] route includes at least one free rune, one combat-gated rune, one
  loot-gated rune, and a final gate
- [x] combat has enemies, player health, attacks, enemy defeat, and visible HUD
  state
- [x] loot chest opens after combat and contributes to progression
- [x] DevAPI smoke proves the full route end to end
- [x] screenshot evidence is nonblank and visually richer than the clean seed
- [x] runtime material pass adds raster texture detail for terrain, stone, wood,
  cloth, and rune surfaces
- [x] model pass improves hero/enemy silhouettes with capes, helmets, shields,
  weapons, faceted accents, and longer directional shadows
- [x] quest chain and world objective marker show current route/progress through
  the vertical slice
- [x] lighting pass adds material shader lighting/fog, denser procedural
  texture detail, local light pools, stronger rim/decal accents, and cleaner
  sun/torch/rune highlights
- [x] authored asset pass adds project-local mesh overlays for hero cuirass,
  cape, helmet crest, enemy masks, and horns
- [ ] final-art bar is not met yet: current models/materials are still
  partly procedural shape-rendered assets, not a full accepted project-local
  3D/textured art set

## Open questions

- Which final art direction should replace the current procedural
  shape-rendered models: toy-like block fantasy, darker low-poly fantasy, or
  Roblox-adjacent bright toy RPG?
- Should the next slice add quests/NPCs, inventory/equipment, or better combat
  feel first?

## Log

- 2026-06-20: user selected the concept as "3д скайрим в мире роблокса" and
  rejected the clean seed quality level.
- 2026-06-20: implemented Blockfell Runes vertical slice in
  `src/clean_seed_main.c`: rune sites, enemy camp, sword attack, health,
  chest/loot, gate, richer blocky environment, and HUD meters.
- 2026-06-20: updated `tools/devapi/smoke.py` to prove first rune -> combat ->
  chest -> remaining runes -> gate open, plus UI and screenshot checks.
- 2026-06-20: evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/smoke.png`.
- 2026-06-20: visual pass added ground shadows, torch/rune glow, stronger
  player/enemy silhouettes, banners, chest metal details, grass/terrain patches,
  crystals, sun rays, and richer ruin/camp props. New showcase evidence:
  `build/captures/blockfell_visual_pass.png`; pixel health passed with
  960x540, 79 sampled unique colors, 62 buckets, luma range 236.2.
- 2026-06-20: material pass added a public `nt_gfx` textured-quad layer with
  procedural raster materials for grass, path, stone, wood, cloth, and rune
  decals. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_material_pass.png`.
  Pixel health: 960x540, 1480 sampled unique colors, 135 buckets.
- 2026-06-20: model pass improved hero/enemy readability with faceted capes,
  helmets/horns, shields, weapons, accent triangles, and longer directional
  shadows. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_model_pass.png`.
  Pixel health: 960x540, 1473 sampled unique colors, 133 buckets.
- 2026-06-20: quest/readability pass added the six-step HUD route chain,
  current objective beam/diamond, route line, camp/chest objective rings, and
  DevAPI `objective_stage` smoke checks. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_quest_pass.png`.
  Pixel health: 960x540, 1496 sampled unique colors, 146 buckets.
- 2026-06-20: lighting/material-depth pass added normal-aware material shader
  lighting/fog, 64px procedural material textures, torch/rune/gate/crystal
  light pools, sun rays, and stronger character decal/rim highlights. Evidence
  passed: `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_lighting_pass.png`.
  Pixel health: 960x540, 2342 sampled unique colors, 158 buckets.
- 2026-06-20: authored asset pass added `src/blockfell_authored_assets.h`,
  `assets/blockfell-runes/asset_manifest.md`, and a separate `nt_gfx` mesh
  pass for project-local hero/enemy overlay meshes. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_authored_asset_pass.png`.
  Pixel health: 960x540, 2423 sampled unique colors, 183 buckets.

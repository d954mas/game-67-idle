---
id: T0013
title: Blockfell Runes first playable vertical slice
status: dropped
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
- [x] authored prop pass adds project-local mesh overlays for rune spires,
  rune glyphs, gate keystone, chest lock plate, and camp standards
- [x] authored environment pass adds project-local mesh overlays for pine
  crowns, rock shards, ruin trims, and path stones
- [x] offline reference pass aligns presentation toward isometric fantasy
  sandbox/ARPG readability with a fixed isometric camera and five-slot action
  belt
- [x] art cleanup pass removes global debug wireframes, visible ground grid, and
  top debug-style status panels
- [x] world art cleanup pass replaces first-frame debug-looking 3D cubes/lines
  with faceted mountains, trees, crystals, camp props, rune stones, gate forms,
  a decorative action belt, and a less flat island silhouette
- [x] character asset pass replaces central hero/enemy cube-body rendering with
  project-original authored body/head/weapon meshes and keeps only shadows,
  weapon feedback, and small combat indicators in the shape pass
- [x] imported texture source pass uses an accepted CC0 Poly Haven ground
  diffuse as a project-local runtime material derivative for terrain overlays
- [x] world depth cleanup pass replaces the square test-floor silhouette with
  an irregular island plateau, removes sky banding, and retunes shadow/glow
  colors away from black debug disks
- [x] authored environment mesh pass moves first-view mountain and tree
  silhouettes out of shape-rendered triangles/cylinders into the project-local
  mesh overlay path
- [x] authored camp/chest mesh pass moves the central combat camp dais,
  canopy, chest body, and chest lid out of shape-rendered quads/cylinders and
  tones down the combat objective ground ring
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
- 2026-06-20: authored prop pass expanded the project-local mesh kit with rune
  spires/glyphs, gate keystone, chest lock plate, and camp standards. Evidence
  passed: `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_prop_asset_pass.png`.
  Pixel health: 960x540, 2478 sampled unique colors, 193 buckets.
- 2026-06-20: authored environment pass expanded the project-local mesh kit
  with pine crowns, rock shards, ruin trims, and path stones, then placed them
  into the first route view. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_environment_asset_pass.png`.
  Pixel health: 960x540, 2763 sampled unique colors, 202 buckets.
- 2026-06-20: user added Albion Online as a reference but explicitly requested
  an offline interpretation. Reference Digest now constrains it to
  isometric/offline fantasy sandbox readability without copying Albion's
  brand, UI, MMO model, economy, or assets. Runtime pass shifted to a more
  isometric camera and added a five-slot action belt plus `ability.belt` DevAPI
  node. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_offline_reference_pass.png`.
  Pixel health: 960x540, 3125 sampled unique colors, 231 buckets.
- 2026-06-20: after lead rejection that the visual still read as debug, art
  cleanup pass removed global cube/triangle wireframes, removed the visible
  ground grid, and stopped drawing top debug-style rune/HP/enemy panels.
  Evidence passed: `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_art_cleanup_pass.png`.
  Pixel health: 960x540, 2978 sampled unique colors, 235 buckets.
- 2026-06-20: after follow-up rejection that the world/3D still looked like
  debug geometry, world art cleanup pass replaced the most visible cube/line
  language in the first combat view: mountains became faceted rock masses,
  block trees became tapered low-poly crowns, crystals became spires, rune/gate
  markers lost wire rings, camp/chest/gate forms became faceted props, the
  terrain gained island side slopes, and the old black HUD controls became a
  single decorated action belt. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_world_art_pass.png`.
  Pixel health: 960x540, 3433 sampled unique colors, 215 buckets.
- 2026-06-20: character asset pass added project-original authored meshes for
  hero body/head/sword and enemy body/head, then removed the central hero/enemy
  cube-body construction from the shape renderer. Shape rendering now keeps
  shadows, weapon/impact feedback, and small combat indicators while the main
  character bodies render through the authored mesh pass. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_character_asset_pass.png`.
  Pixel health: 960x540, 3721 sampled unique colors, 257 buckets.
- 2026-06-20: imported texture source pass copied accepted CC0 Poly Haven
  `brown_mud_leaves_01_diff_1k.jpg` into project-local source assets,
  generated `src/blockfell_texture_assets.h`, and changed the terrain material
  overlay to use the 64x64 RGBA runtime derivative instead of procedural grass
  noise. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_texture_source_pass.png`.
  Pixel health: 960x540, 4054 sampled unique colors, 250 buckets.
- 2026-06-20: world depth cleanup pass removed the visible sky band, replaced
  the square world slab with an irregular island plateau and side slopes, and
  changed shadow/glow ground overlays to muted terrain-tinted colors so the
  3D view no longer reads as black debug disks on a test floor. Evidence
  passed: `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_lighting_depth_pass.png`.
  Pixel health: 960x540, 4586 sampled unique colors, 249 buckets.
- 2026-06-20: authored environment mesh pass added project-original pine trunk,
  mountain body, and mountain snow-cap meshes, then moved the first-view tree
  and mountain silhouettes from shape-rendered triangles/cylinders into the
  mesh overlay path with non-uniform mesh scaling. Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_environment_mesh_pass.png`.
  Pixel health: 960x540, 6701 sampled unique colors, 271 buckets.
- 2026-06-20: authored camp/chest mesh pass added project-original camp dais,
  camp canopy, chest body, and chest lid meshes, removed the old camp/chest
  shape-rendered quads/cylinders from the first combat view, and toned down the
  combat objective ground ring so it no longer reads as a bright debug overlay.
  Evidence passed: `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py 9123`, and
  `py -3.12 tools/devapi/pixel_health.py build/captures/blockfell_camp_mesh_pass.png`.
  Pixel health: 960x540, 7017 sampled unique colors, 278 buckets.
- 2026-06-20: Closed Blockfell Runes before clean new-game iteration.

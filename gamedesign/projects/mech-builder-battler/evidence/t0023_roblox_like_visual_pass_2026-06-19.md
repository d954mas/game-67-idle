---
title: T0023 Roblox-like visual pass evidence
description: Screenshot and validation record for the block-world mech visual pivot.
tags: [project, evidence, visual, 3d, mechs, roblox-like]
updated: 2026-06-19
---

# T0023 Roblox-Like Visual Pass Evidence

## What Changed

- Replaced the dark industrial floor read with a bright green block baseplate
  world: tiled panels, raised studs, stylized grass/leaf line motifs, colored
  block props, simple sky color, and raised hangar/battle pad blocks.
- Shifted the starter mech toward a toy/block grammar: brighter painted-plastic
  palette, larger chunky arms/legs/feet, fewer dark thin supports, more cyan
  module accents, and shoulder rocket modules that still read after purchase.
- Added new project-owned glTF source shapes for armor plates, visor plates,
  low-poly joints, and hydraulic/cylindrical parts through the existing mesh
  pack route.
- Added a reusable standalone texture-generation skill for future generated or
  downloaded texture work, including tiling decisions and source provenance.
- Integrated a downloaded CC0 hero mech source model from Poly Pizza/Quaternius
  as the visible runtime mech body. The previous procedural block mech remains
  packed as fallback/kitbash source, but the current render path hides it.
- Extracted the model's embedded `Atlas.png` from the GLB, packed it as a
  runtime `NT_ASSET_TEXTURE`, added `TEXCOORD_0` to the source mech stream, and
  changed the mech shader to light the atlas color instead of tinting the whole
  model with one flat instance color.
- Added raised studs to the hangar pad and toy-block props so the world reads
  more like a physical block/baseplate set in perspective, not only a flat
  checker texture.
- Added visible shoulder hardpoints to the downloaded hero mech: locked orange
  sockets before purchase and amber rocket pods with cyan caps after purchase.
  The overlay is a runtime module-readability pass, not a final authored mesh.
- Downloaded a second CC0 Quaternius source candidate, `Robot Enemy Legs Gun`,
  for future modular robot/mech part sourcing. It is recorded as source
  provenance only and is not currently packed into runtime.

## Screenshot Evidence

- Hangar: `build/captures/mech_t0021_hangar_smoke.png`
- Battle: `build/captures/mech_t0021_battle_smoke.png`
- Rocket-equipped battle: `build/captures/mech_t0021_rockets_smoke.png`

Visual judgment:

- The scene now reads as a bright block/toy arena instead of a dark tech demo.
- The ground direction matches the user-provided stylized-studs reference well
  enough for a runtime proof: studs are visible, motifs break the grid, and the
  surface is more custom than a plain baseplate.
- The mech is improved versus the previous "set of cubes" state: limbs are
  chunkier and more intentional, and the body has clearer toy-machine mass.
- After the downloaded model integration, the player mech now reads as a
  Roblox-like toy mech/suit silhouette with built-in cannons and mechanical
  legs, rather than a project-authored pile of blocks.
- After the atlas integration, the mech keeps the downloaded model's green
  armor, gray cannons, dark joints, and orange/brown accent parts. This closes
  the previous single-green-material caveat for the current CC0 asset.
- The new raised studs make the hangar pad and prop blocks read more like
  chunky toy construction pieces at the native camera distance.
- The rocket purchase now changes the mech silhouette in the screenshot: the
  shoulder sockets visibly become rocket pods instead of only changing the HUD
  button.

## Residual Risk

- This is still not final hero mech quality. The next art pass should keep using
  permissively licensed downloaded/source assets where they beat procedural
  shapes, and either find a stronger Roblox-like mech source or create a more
  deliberate block-kit model with clearer customization slots before expanding
  combat.
- The current hardpoints are a readable runtime overlay. A later final model
  should turn them into authored mesh parts or a modular GLB set.
- The stylized-studs ground is procedural proof, not yet a shippable texture
  asset. A future texture pass should decide tileable vs non-tileable up front
  and record provenance using `game-texture-generation`.
- The Quaternius atlas is a 32x32 low-poly palette/atlas, not a rich material
  set with normals/roughness/emissive maps. It is correct for the downloaded
  asset, but a future higher-quality mech may need a stronger material source.

## Validation

- `cmake --build --preset native-debug --target game_seed`: pass.
- `py -3.12 tools/mech-builder-battler/devapi_playable_smoke.py 9124`: pass.
- `mesh_mech_ready`: pass in DevAPI smoke after texture material integration.
- Strict visual product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T17-37-47-327Z_desktop-hangar.md`,
  pass with all visual scores at 4/5 and no major issue.
- Rocket-module visual product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T17-52-23-263Z_desktop-rocket-modules.md`,
  pass with all visual scores at 4/5 and no major issue.
- `.codex/skills/game-texture-generation`: `quick_validate.py` pass.

## Downloaded Asset Provenance

- Asset: `Mech` by Quaternius on Poly Pizza.
- Source page: `https://poly.pizza/m/o3Ps8z8ByP`.
- Downloaded preview: `assets/source/models/quaternius/poly_pizza_quaternius_mech_cc0_preview.jpg`.
- Downloaded source GLB: `assets/source/models/quaternius/poly_pizza_quaternius_mech_cc0.glb`.
- Extracted source atlas: `assets/source/models/quaternius/poly_pizza_quaternius_mech_cc0_atlas.png`.
- Runtime mesh copy: `assets/meshes/poly_pizza_quaternius_mech_cc0.glb`.
- Runtime atlas: `assets/textures/poly_pizza_quaternius_mech_cc0_atlas.png`.
- License as shown on source page on 2026-06-19: Public Domain (CC0 1.0).
- Texture usage class: mech asset material, unique non-tileable atlas driven by
  the GLB UVs. It should not be reused as a world/baseplate tile.

Additional source candidate:

- Asset: `Robot Enemy Legs Gun` by Quaternius on Poly Pizza.
- Source page: `https://poly.pizza/m/lFZfDh2hzP`.
- Downloaded preview: `assets/source/models/quaternius/poly_pizza_quaternius_robot_enemy_legs_gun_cc0_preview.jpg`.
- Downloaded source GLB: `assets/source/models/quaternius/poly_pizza_quaternius_robot_enemy_legs_gun_cc0.glb`.
- License as shown on source page on 2026-06-19: Public Domain (CC0 1.0).
- Runtime status: source candidate only; not packed or rendered in the current
  slice.

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

## Residual Risk

- This is still not final hero mech quality. The next art pass should use either
  a stronger downloaded/permissively licensed model source or a more deliberate
  block-kit modeling pass with better silhouette design before expanding combat.
- The stylized-studs ground is procedural proof, not yet a shippable texture
  asset. A future texture pass should decide tileable vs non-tileable up front
  and record provenance using `game-texture-generation`.
- The Quaternius model's original color separation is in its GLB material
  texture atlas. The current runtime mesh path only uses position/normal plus an
  instance color, so the model is temporarily tinted as a single green toy
  material. A follow-up texture/material integration pass should restore the
  atlas or rebuild the material colors through a supported engine path.

## Validation

- `cmake --build --preset native-debug --target game_seed`: pass.
- `py -3.12 tools/mech-builder-battler/devapi_playable_smoke.py 9124`: pass.
- `.codex/skills/game-texture-generation`: `quick_validate.py` pass.

## Downloaded Asset Provenance

- Asset: `Mech` by Quaternius on Poly Pizza.
- Source page: `https://poly.pizza/m/o3Ps8z8ByP`.
- Downloaded preview: `assets/source/models/quaternius/poly_pizza_quaternius_mech_cc0_preview.jpg`.
- Downloaded source GLB: `assets/source/models/quaternius/poly_pizza_quaternius_mech_cc0.glb`.
- Runtime mesh copy: `assets/meshes/poly_pizza_quaternius_mech_cc0.glb`.
- License as shown on source page on 2026-06-19: Public Domain (CC0 1.0).
- Runtime caveat: mesh is used; original texture atlas/color material is not yet
  restored in the runtime shader.

# T0024 CC0 Robot Enemy Asset Runtime Pass

## Result

The battle scene now uses the downloaded Poly Pizza/Quaternius `Robot Enemy
Legs Gun` CC0 source asset as the visible robot/drone enemy read. The runtime no
longer relies on sphere/debug drone bodies when the mesh path is ready.

## Visual Changes

- Split the source GLB into seven static runtime meshes by source material:
  grey body, orange shell, pale edge, dark details, red eye, grey gun, and
  light-grey gun trim.
- Baked GLB node transforms during extraction so the source pose, scale
  relationship, and weapon placement survive the runtime pack path.
- Added a color-only instanced mesh fragment shader for robot enemy parts so
  source material colors are not polluted by the hero mech atlas.
- Raised entity/component capacities for the mesh entity count and expanded the
  render-item list for hero plus enemy parts.
- Repositioned the first enemy wave so the first battle screenshot shows a
  readable robot target instead of hiding all enemies behind the player mech.

## Asset Provenance

- Source asset: `Robot Enemy Legs Gun` by Quaternius on Poly Pizza.
- Source URL: https://poly.pizza/m/lFZfDh2hzP
- License observed on 2026-06-19: Public Domain CC0 1.0.
- Downloaded preview:
  `assets/source/models/quaternius/poly_pizza_quaternius_robot_enemy_legs_gun_cc0_preview.jpg`.
- Downloaded source GLB:
  `assets/source/models/quaternius/poly_pizza_quaternius_robot_enemy_legs_gun_cc0.glb`.
- Runtime derived meshes:
  `assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_*_static_cc0.gltf`.

## Evidence

- Native build: `cmake --build --preset native-debug --target game_seed`.
- DevAPI smoke: `py -3.12 tools/mech-builder-battler/devapi_playable_smoke.py 9124`.
- Main screenshot:
  `build/captures/mech_t0024_robot_enemy_asset_smoke.png`.
- Smoke log:
  `build/logs/native_devapi_9124_20260619_234555_652.log`.

## Remaining Visual Gap

The enemy asset is now recognizable as a low-poly robot/drone in battle, but the
player hero mech remains the bigger visual priority. The next visual slice
should improve the hero mech's Roblox-like modular silhouette and add stronger
toy/plastic material language.

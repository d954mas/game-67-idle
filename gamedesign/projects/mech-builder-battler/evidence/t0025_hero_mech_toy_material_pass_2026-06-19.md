# T0025 Hero Mech Toy Material Pass

## Result

The player hero mech now uses a generated toy/plastic runtime atlas derived
from the downloaded CC0 Poly Pizza/Quaternius `Mech` source atlas. The first
hangar screenshot reads brighter and more Roblox-like, with blue-tinted plastic
metal, mint armor, orange module accents, and stronger specular/rim lighting.

## Texture Contract

- Usage class: unique mech material / asset material.
- Tiling: not required. The source GLB owns the UVs and samples a tiny palette
  atlas; the runtime texture is not used as a repeating world material.
- Source route: downloaded CC0 source atlas, transformed by a local deterministic
  recolor script.
- Source atlas:
  `assets/source/models/quaternius/poly_pizza_quaternius_mech_cc0_atlas.png`.
- Runtime atlas:
  `assets/textures/poly_pizza_quaternius_mech_toy_atlas.png`.
- Recolor script:
  `tools/mech-builder-battler/recolor_mech_atlas.py`.

## Visual Changes

- Added a reproducible build step that creates the runtime toy atlas from the
  source atlas before packing.
- Updated the mesh pack and runtime resource request to use
  `poly_pizza_quaternius_mech_toy_atlas.png`.
- Increased the hero mesh shader's toy/plastic feel with stronger value lift,
  saturation, specular, rim light, and a softer toon-like diffuse band.
- Preserved the existing CC0 hero GLB, Y-up runtime pose, engine font UI, and
  DevAPI smoke path.

## Evidence

- Native build: `cmake --build --preset native-debug --target game_seed`.
- DevAPI smoke: `py -3.12 tools/mech-builder-battler/devapi_playable_smoke.py 9124`.
- Main screenshot:
  `build/captures/mech_t0025_hero_toy_material_smoke.png`.
- Battle regression screenshot:
  `build/captures/mech_t0024_robot_enemy_asset_smoke.png`.
- Smoke log:
  `build/logs/native_devapi_9124_20260619_235631_321.log`.

## Remaining Visual Gap

The material pass improves first-screen readability, but the hero model is still
the same downloaded low-poly mech. The next large upgrade should address
modular/rigged body structure, clearer physical slots, and stronger authored
animation instead of only material treatment.

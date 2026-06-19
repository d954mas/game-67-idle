# T0036 Kenney Modular Space World Props Evidence

Date: 2026-06-20

## Result

T0036 imports a small CC0 subset from Kenney `Modular Space Kit` and renders it
as sourced sci-fi station world dressing in the native mech hangar. This is the
next asset-first visual correction after the sourced mech showcase work: the
world should read as a toy/block sci-fi place, not only a procedural test floor.

## Provenance

- Source page: `https://kenney.nl/assets/modular-space-kit`
- Download archive:
  `https://kenney.nl/media/pages/assets/modular-space-kit/8261428a47-1771146076/kenney_modular-space-kit_1.0.zip`
- Author/source: Kenney.
- Package: `Modular Space Kit (1.0)`.
- License observed in local package: Creative Commons Zero / CC0.
- Attribution required: no.
- Local source license:
  `assets/source/models/kenney/modular_space_kit/License.txt`
- Local source preview:
  `assets/source/models/kenney/modular_space_kit/kenney_modular_space_kit_preview.png`

## Runtime Assets

- `assets/meshes/kenney_modular_space_gate_cc0.glb`
- `assets/meshes/kenney_modular_space_corridor_wide_cc0.glb`
- `assets/meshes/kenney_modular_space_room_small_cc0.glb`

## Runtime Integration

- Render integration: `src/clean_seed_main.c`
- Pack integration: `tools/mech-builder-battler/build_packs.c`
- Build dependency integration: `CMakeLists.txt`
- Smoke capture integration:
  `tools/mech-builder-battler/devapi_playable_smoke.py`

## Screenshot Evidence

- After T0036:
  `build/captures/mech_t0036_kenney_space_props_hangar_smoke.png`

## Product Gate

- Markdown:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-15-10_desktop-kenney-space-world-props.md`
- JSON:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-15-10_desktop-kenney-space-world-props.json`
- Verdict: PASS, strict visual scores all 4/5.

## Validation

Commands:

```powershell
cmake --build --preset native-debug --target game_seed
py -3.12 tools\mech-builder-battler\devapi_playable_smoke.py 9124
node tools\product_gate\review.mjs --project mech-builder-battler --task T0036 --surface desktop-kenney-space-world-props --screenshot build\captures\mech_t0036_kenney_space_props_hangar_smoke.png --verdict pass --strict --visual-strict
node tools\taskboard\cli.mjs validate
node tools\ai.mjs validate --with-assets
git diff --check
```

Observed build evidence:

- CMake built `game_seed` successfully.
- Pack builder encoded `60 assets`, including `MESH: 50 assets`.
- Pack builder listed:
  - `assets/meshes/kenney_modular_space_gate_cc0.glb`
  - `assets/meshes/kenney_modular_space_corridor_wide_cc0.glb`
  - `assets/meshes/kenney_modular_space_room_small_cc0.glb`
- DevAPI smoke passed required endpoints, mesh readiness, hangar UI, battle
  start, WASD movement, reward, upgrade, retest, and rocket attack checks.
- Screenshot captured:
  `build/captures/mech_t0036_kenney_space_props_hangar_smoke.png`

## Remaining Visual Debt

- First pass uses runtime tint/materials rather than authored material
  separation per prop.
- The world still needs more coherent Roblox-like layout composition, foreground
  landmarks, and stronger prop/floor lighting balance.
- A next pass should add either a stronger blocky/mech asset source or more
  authored texture/material treatment for the world kit.

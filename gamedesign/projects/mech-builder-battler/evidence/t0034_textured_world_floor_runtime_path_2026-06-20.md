# T0034 Textured World Floor Runtime Path Evidence

Date: 2026-06-20

## Result

T0034 moves the stylized-studs grass floor from source/provenance proof into a
real native runtime path. The game now packs a Y-up floor mesh with repeating
UVs, packs the stylized-studs texture as a texture asset, requests a dedicated
world material, and renders that mesh before the mech/enemy items.

## Runtime Assets

- Source texture:
  `assets/source/textures/mech-builder-battler/stylized_studs_grass_tile_v1.png`
- Runtime texture:
  `assets/textures/mech_builder_battler_stylized_studs_grass_tile_v1.png`
- Runtime mesh:
  `assets/meshes/mech_world_studs_floor.gltf`
- Mesh generator:
  `tools/mech-builder-battler/generate_world_floor_mesh.py`
- Runtime integration:
  `src/clean_seed_main.c`
- Pack integration:
  `tools/mech-builder-battler/build_packs.c`

## Screenshot Evidence

- Hangar:
  `build/captures/mech_t0034_textured_floor_hangar_smoke.png`
- Battle entry:
  `build/captures/mech_t0034_textured_floor_battle_entry_smoke.png`
- Battle after movement/combat setup:
  `build/captures/mech_t0034_textured_floor_battle_smoke.png`

## Product Gate

- Markdown:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T01-55-10_desktop-textured-floor-runtime.md`
- JSON:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T01-55-10_desktop-textured-floor-runtime.json`
- Verdict: PASS, strict visual scores all 4/5.

## Validation

Commands:

```powershell
py -3.12 tools\mech-builder-battler\generate_world_floor_mesh.py
cmake --build --preset native-debug --target game_seed
py -3.12 tools\mech-builder-battler\devapi_playable_smoke.py 9124
node tools\product_gate\review.mjs --project mech-builder-battler --task T0034 --surface desktop-textured-floor-runtime --screenshot build\captures\mech_t0034_textured_floor_hangar_smoke.png --verdict pass --strict --visual-strict
```

Observed build evidence:

- Pack builder encoded `assets/meshes/mech_world_studs_floor.gltf`.
- Pack builder encoded
  `assets/textures/mech_builder_battler_stylized_studs_grass_tile_v1.png`.
- Pack summary included `MESH: 37 assets` and `TEX: 2 assets`.
- DevAPI smoke passed required endpoints, mesh readiness, hangar UI, battle
  start, WASD movement, reward, upgrade, retest, and rocket attack checks.

## Remaining Visual Debt

- The first runtime floor material is intentionally bright and still needs a
  polish pass for color balance, material tuning, mips, and likely
  normal/roughness support.
- The packed texture is currently reported as 4.0M by the pack builder; a later
  texture optimization pass should resize/compress or add proper mip handling.
- This slice does not replace the mech model. The next visual priority remains
  asset-first Roblox-like mech/world sourcing and integration.

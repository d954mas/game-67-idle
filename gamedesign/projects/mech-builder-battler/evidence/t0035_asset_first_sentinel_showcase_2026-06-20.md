# T0035 Asset-First Sentinel Showcase Evidence

Date: 2026-06-20

## Result

T0035 adds a second real downloaded mech source asset to the hangar: Poly Pizza
`Sentinel Mech` by Tekano Bob. The model is stored as source GLB/preview,
material-split into native packable GLTF meshes, packed into the native asset
pack, and rendered as a side-pad showroom/rival mech next to the main Assault
Walker.

This is an asset-first correction to the earlier cube/procedural direction. The
new mech is not a final playable/animated unit yet.

## Provenance

- Source page: `https://poly.pizza/m/aGSpAN8ONud`
- Source GLB:
  `https://static.poly.pizza/40d7525e-237c-448b-8b1d-8260bd0b1885.glb`
- Preview:
  `https://static.poly.pizza/40d7525e-237c-448b-8b1d-8260bd0b1885.jpg`
- Author/source: Tekano Bob / Poly Pizza.
- License observed: Creative Commons Attribution / CC-BY 3.0.
- Attribution required: yes, before any public release.
- Local source GLB:
  `assets/source/models/poly_pizza/tekano_bob/poly_pizza_tekano_sentinel_mech_ccby30.glb`
- Local source preview:
  `assets/source/models/poly_pizza/tekano_bob/poly_pizza_tekano_sentinel_mech_ccby30_preview.jpg`

## Runtime Assets

- Extracted runtime mesh parts:
  `assets/meshes/poly_pizza_tekano_sentinel_mech_*_static_ccby30.gltf`
- Runtime integration:
  `src/clean_seed_main.c`
- Pack integration:
  `tools/mech-builder-battler/build_packs.c`
- CMake extraction path:
  `CMakeLists.txt`

## Screenshot Evidence

- Before/reference baseline:
  `build/captures/mech_t0034_textured_floor_hangar_smoke.png`
- After T0035:
  `build/captures/mech_t0035_asset_first_sentinel_hangar_smoke.png`

## Product Gate

- Markdown:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-05-10_desktop-asset-first-sentinel-showcase.md`
- JSON:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-05-10_desktop-asset-first-sentinel-showcase.json`
- Verdict: PASS, strict visual scores all 4/5.

## Validation

Commands:

```powershell
cmake --build --preset native-debug --target game_seed
py -3.12 tools\mech-builder-battler\devapi_playable_smoke.py 9124
node tools\product_gate\review.mjs --project mech-builder-battler --task T0035 --surface desktop-asset-first-sentinel-showcase --screenshot build\captures\mech_t0035_asset_first_sentinel_hangar_smoke.png --verdict pass --strict --visual-strict
```

Observed build evidence:

- CMake regenerated 10 Sentinel material-split static GLTF files.
- Pack builder encoded `57 assets`, including `MESH: 47 assets`.
- Pack builder listed all 10
  `assets/meshes/poly_pizza_tekano_sentinel_mech_*_static_ccby30.gltf` parts.
- DevAPI smoke passed required endpoints, mesh readiness, hangar UI, battle
  start, WASD movement, reward, upgrade, retest, and rocket attack checks.

## Remaining Visual Debt

- Sentinel is a static showroom/rival mech. It still needs authored animation,
  damage/upgrade states, and a gameplay role before it is a full mech unit.
- CC-BY attribution must appear in credits or another public-release
  attribution surface before shipping.
- The next asset-first slice should try an animated mech source or a stronger
  CC0/CC-BY world prop set instead of adding procedural block props.

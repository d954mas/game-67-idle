# T0037 Station Plastic World Composition Evidence

Date: 2026-06-20

## Result

T0037 keeps the T0036 asset-first world path and improves the hangar by reusing
the same downloaded CC0 Kenney station props as additional landmarks. The props
now use a dedicated plastic/station shader instead of the generic solid overlay
material, making the first screen read more like a bright toy sci-fi hangar.

## Runtime Changes

- Added shader:
  `assets/shaders/mech_mesh_station_inst.frag`
- Added dedicated runtime material:
  `kenney_station_plastic_props`
- Increased Kenney world prop runtime placement from 3 sourced-prop instances
  to 8 instances, still using the same 3 CC0 runtime meshes.
- Added DevAPI smoke capture:
  `build/captures/mech_t0037_station_plastic_hangar_smoke.png`

## Screenshot Evidence

- Before/reference:
  `build/captures/mech_t0036_kenney_space_props_hangar_smoke.png`
- After T0037:
  `build/captures/mech_t0037_station_plastic_hangar_smoke.png`

## Product Gate

- Markdown:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-24-40_desktop-station-plastic-world-composition.md`
- JSON:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-24-40_desktop-station-plastic-world-composition.json`
- Verdict: PASS, strict visual scores all 4/5.

## Validation

Commands:

```powershell
cmake --build --preset native-debug --target game_seed
py -3.12 tools\mech-builder-battler\devapi_playable_smoke.py 9124
node tools\product_gate\review.mjs --project mech-builder-battler --task T0037 --surface desktop-station-plastic-world-composition --screenshot build\captures\mech_t0037_station_plastic_hangar_smoke.png --verdict pass --strict --visual-strict
node tools\taskboard\cli.mjs validate
node tools\ai.mjs validate --with-assets
git diff --check
```

Observed evidence:

- CMake built `game_seed` successfully.
- Pack builder encoded `61 assets`, including `SHADER: 8 assets`.
- Pack builder validated `assets/shaders/mech_mesh_station_inst.frag` for GL
  3.30 and ES 3.00.
- DevAPI smoke passed required endpoints, mesh readiness, hangar UI, battle
  start, WASD movement, reward, upgrade, retest, and rocket attack checks.

## Remaining Visual Debt

- The shader adds toy gloss and panel rhythm, but the Kenney props still need
  true material masks/section colors for richer sourced-asset detail.
- The next high-value visual slice should return to the mech itself: better
  authored material separation, stronger attack/motion readability, or a new
  legal animated mech source.

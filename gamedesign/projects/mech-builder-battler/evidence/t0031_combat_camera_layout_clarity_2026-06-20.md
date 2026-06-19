# T0031 Combat Camera And Layout Clarity Pass

## Result

The sourced CC-BY Assault Walker now reads cleaner in the active battle
screenshot. This pass keeps the asset-first direction and improves presentation
around the imported mech rather than adding new combat or progression scope.

## What Changed

- Pulled the battle camera slightly back and narrowed the battle FOV from 58 to
  55 degrees.
- Raised and shifted the battle look-at point so the Assault Walker silhouette
  is less swallowed by foreground grid/effects.
- Moved/lowered the right-front arena pylon in battle mode to reduce HUD and
  foreground pressure.
- Reduced stomp rings, muzzle glow, target beams, vent trails, and target
  charge intensity so effects support the mech instead of covering it.
- Added `mech_t0031_combat_clarity_battle_smoke.png` to the DevAPI smoke.

## Screenshot Evidence

- T0031 battle clarity:
  `build/captures/mech_t0031_combat_clarity_battle_smoke.png`
- Previous T0030 battle material pop comparison:
  `build/captures/mech_t0030_lighting_material_battle_smoke.png`
- Product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T01-20-10_desktop-combat-camera-layout-clarity.md`

## Validation

```powershell
cmake --build --preset native-debug --target game_seed
py -3.12 tools\mech-builder-battler\devapi_playable_smoke.py 9124
node tools\taskboard\cli.mjs validate
node tools\product_gate\review.mjs --project mech-builder-battler --task T0031 --surface desktop-combat-camera-layout-clarity --screenshot build\captures\mech_t0031_combat_clarity_battle_smoke.png --verdict pass --strict --visual-strict ...
```

## Review Notes

- Strict visual gate passed with composition/readability/ui/action/art/audience
  scores at 4.
- Remaining visual debt is explicit: the hero still needs authored or kitbashed
  limb articulation and richer sourced/generated textures after the current
  readability pass.

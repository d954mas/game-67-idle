# T0038 Hero Mech Material Attack Read Evidence

Date: 2026-06-20

## Result

T0038 returns focus from world dressing to the player mech. The sourced Assault
Walker hero now has stronger toy-plastic color zones, brighter cockpit/weapon
accents, heat/attack color response, and a slightly closer battle camera so the
hero reads better at gameplay size.

## Runtime Changes

- Tuned Assault Walker material-split tint palette in `src/clean_seed_main.c`.
- Added stronger attack/heat glow response for cockpit, dark mechanical zones,
  and weapon/accent parts.
- Increased battle visual hero scale from `0.88` to `0.96`.
- Tightened battle camera/FOV for a larger, clearer mech read.
- Added DevAPI smoke capture:
  `build/captures/mech_t0038_hero_material_attack_read_smoke.png`

## Screenshot Evidence

- Before/reference:
  `build/captures/mech_t0037_station_plastic_hangar_smoke.png`
- After T0038:
  `build/captures/mech_t0038_hero_material_attack_read_smoke.png`

## Product Gate

- Markdown:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-32-20_desktop-hero-mech-material-attack-read.md`
- JSON:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-32-20_desktop-hero-mech-material-attack-read.json`
- Verdict: PASS, strict visual scores all 4/5.

## Validation

Commands:

```powershell
cmake --build --preset native-debug --target game_seed
py -3.12 tools\mech-builder-battler\devapi_playable_smoke.py 9124
node tools\product_gate\review.mjs --project mech-builder-battler --task T0038 --surface desktop-hero-mech-material-attack-read --screenshot build\captures\mech_t0038_hero_material_attack_read_smoke.png --verdict pass --strict --visual-strict
node tools\taskboard\cli.mjs validate
node tools\ai.mjs validate --with-assets
git diff --check
```

Observed evidence:

- CMake built `game_seed` successfully.
- DevAPI smoke passed required endpoints, mesh readiness, hangar UI, battle
  start, WASD movement, reward, upgrade, retest, and rocket attack checks.
- Corrected the T0038 capture placement after an initial screenshot landed on
  the reward overlay instead of the battle/action state.

## Remaining Visual Debt

- The hero still needs authored material masks/sections instead of runtime tint
  alone.
- Attack quality still depends on simple VFX and recoil; a later pass should
  add stronger authored animation states or a better animated legal source.

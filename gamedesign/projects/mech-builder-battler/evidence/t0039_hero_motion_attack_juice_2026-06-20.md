# T0039 Hero Motion Attack Juice Evidence

Date: 2026-06-20

## Result

T0039 improves the sourced Assault Walker hero's live combat read. The mech now
leans from local movement relative to facing, and battle VFX adds clearer stride
rings, motion trails, and cannon muzzle streaks. This makes the hero feel more
alive without changing combat rules or economy.

## Runtime Changes

- `src/clean_seed_main.c`
  - Changed hero strafe/forward lean to use local velocity relative to facing.
  - Added movement trail lines behind the mech during stride/stomp.
  - Added brighter cannon muzzle streaks alongside existing target lines.
- `tools/mech-builder-battler/devapi_playable_smoke.py`
  - Added `mech_t0039_hero_motion_attack_smoke.png` capture.

## Screenshot Evidence

- After T0039:
  `build/captures/mech_t0039_hero_motion_attack_smoke.png`

## Product Gate

- Markdown:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-39-10_desktop-hero-motion-attack-juice.md`
- JSON:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-39-10_desktop-hero-motion-attack-juice.json`
- Verdict: PASS, strict visual scores all 4/5.

## Validation

Commands:

```powershell
cmake --build --preset native-debug --target game_seed
py -3.12 tools\mech-builder-battler\devapi_playable_smoke.py 9124
node tools\product_gate\review.mjs --project mech-builder-battler --task T0039 --surface desktop-hero-motion-attack-juice --screenshot build\captures\mech_t0039_hero_motion_attack_smoke.png --verdict pass --strict --visual-strict
node tools\taskboard\cli.mjs validate
node tools\ai.mjs validate --with-assets
git diff --check
```

Observed evidence:

- CMake built `game_seed` successfully.
- DevAPI smoke passed required endpoints, mesh readiness, hangar UI, battle
  start, WASD movement, reward, upgrade, retest, and rocket attack checks.

## Remaining Visual Debt

- The mech still needs authored animation states or a legal animated mech
  source; runtime rings/lines are a visual bridge, not final animation.
- Battle framing can keep the hero more centered during diagonal movement.

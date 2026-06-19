# Project Status

## Current Goal

Active game: `Mech Builder Battler` (`mech-builder-battler`): casual
mobile/web-targeted 3D mech builder battler, implemented and iterated first in
the native PC harness.

Project wiki: `gamedesign/projects/mech-builder-battler/README.md`.
Latest control note: `gamedesign/projects/mech-builder-battler/references/mobile_control_patterns_2026-06-19.md`.
Readiness/prototype plan:
`gamedesign/projects/mech-builder-battler/design/reference_readiness_and_prototype_plan_2026-06-19.md`.

## Current Runtime Surface

Native `game_seed` is the clean seed work surface:

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
```

Runtime entrypoint: `src/clean_seed_main.c` (362-line debug template).

The engine submodule at `external/neotolis-engine` is read-only from this repo.
Reusable sidecar modules, tools, skills, and game code may be edited here.

## Current Gate

First native PC prototype is open via `T0021`. Exact UI/economy/combat pacing
and final reference-driven art remain gated behind current-build screenshots
and stronger reference evidence (`T0022`). Implementation/iteration remains
native PC; mobile/web is the UX/export target. Accepted: one owned mech, PvE
first, semi-auto arena, floating joystick/WASD, resources -> hangar
purchase/craft, shoulder rockets, drone swarm proof, short dash, `Cooling` UI
label, fixed three-quarter/isometric camera, landscape-first, industrial
salvage sport, Foundry Warden mini-boss.

## Next Priorities

1. Continue `T0021`: improve the first native PC playable slice visual quality
   from primitive-built prototype toward model-like mech presentation.
2. Keep the current screenshot evidence:
   `gamedesign/projects/mech-builder-battler/evidence/t0021_runtime_visual_review_2026-06-19.md`.
3. Run `T0022`: update Mech Arena/CATS/Mechangelion mismatch audits against
   the first native capture.
4. Defer web/mobile export work until explicitly approved; preserve
   mobile-style controls/readability in the PC harness.

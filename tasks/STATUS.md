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

Runtime entrypoint: `src/clean_seed_main.c`.

The engine submodule at `external/neotolis-engine` is read-only from this repo.
Reusable sidecar modules, tools, skills, and game code may be edited here.

## Current Gate

First native PC prototype slice `T0021` is done and `T0022` refreshed the
reference mismatch audit against current screenshots. The loop works, DevAPI
smoke passes, and the strict product gate is PASS after the starter mech moved
to a mesh/material/normal path. Art debt remains: `T0023` must replace the
cube-kitbashed starter with an authored/high-fidelity mech. Iteration remains
native PC; mobile/web is the UX/export target. Accepted: one owned mech, PvE
first, semi-auto arena, floating joystick/WASD, resources -> hangar
purchase/craft, shoulder rockets, drone swarm proof, short dash, `Cooling`, a
fixed three-quarter/isometric camera, industrial salvage sport, and Foundry
Warden mini-boss.

## Next Priorities

1. Run `T0023`: replace the cube-kitbashed starter with an authored or
   generated high-fidelity mech asset using the proven mesh/material pack path.
2. Keep reference constraints from
   `gamedesign/projects/mech-builder-battler/references/current_build_mismatch_audit_2026-06-19.md`.
3. Keep product evidence in
   `gamedesign/projects/mech-builder-battler/evidence/t0021_runtime_visual_review_2026-06-19.md`.
4. Defer web/mobile export work until explicitly approved; preserve
   mobile-style controls/readability in the PC harness.

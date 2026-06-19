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
reference mismatch audit against current screenshots. The loop works and DevAPI
smoke passes. The art target has pivoted: `T0023` is now P0 and must improve the
visual, Roblox-like world, and starter mech until the player no longer reads as
a simple set of cubes. Iteration remains native PC; mobile/web is the UX/export
target. Accepted: one owned mech, PvE first, semi-auto arena, floating
joystick/WASD, resources -> hangar purchase/craft, shoulder rockets, drone swarm
proof, short dash, `Cooling`, a fixed three-quarter/isometric camera, bright
Roblox-like toy/block world, stylized studs surfaces, and Foundry Warden
mini-boss.

## Next Priorities

1. Run `T0023`: improve the mech and Roblox-like world as the first priority.
   The mech must read as a deliberate toy/block mech character, not a cube pile.
   Current pass uses a CC0 Poly Pizza/Quaternius mech with restored atlas color
   and raised studs on the pad/props.
2. Keep reference constraints from
   `gamedesign/projects/mech-builder-battler/references/current_build_mismatch_audit_2026-06-19.md`.
3. Keep product evidence in
   `gamedesign/projects/mech-builder-battler/evidence/t0021_runtime_visual_review_2026-06-19.md`.
4. Defer web/mobile export work until explicitly approved; preserve
   mobile-style controls/readability in the PC harness.

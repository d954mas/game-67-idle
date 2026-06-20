# Project Status

## Current Goal

Active game: `Mech Builder Battler` (`mech-builder-battler`): casual
mobile/web-targeted 3D mech builder battler, implemented and iterated first in
the native PC harness.

Project wiki: `gamedesign/projects/mech-builder-battler/README.md`.
Latest control note:
`gamedesign/projects/mech-builder-battler/references/mobile_control_patterns_2026-06-19.md`.

## Current Runtime Surface

Native `game_seed` is the work surface:

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
```

Runtime entrypoint: `src/clean_seed_main.c`.

The engine submodule at `external/neotolis-engine` is read-only from this repo.
Reusable sidecar modules, tools, skills, and game code may be edited here.

## Current Gate

`T0021` is done, `T0022` refreshed the mismatch audit, and `T0023` / `T0025` /
`T0026` / `T0027` / `T0028` / `T0029` / `T0030` / `T0031` / `T0032` / `T0033` /
`T0034` / `T0035` / `T0036` / `T0037` / `T0038` / `T0039` are in review after
asset-first Roblox-like visual passes. `T0040` is doing after lead feedback:
the mech still reads like a static plastic figurine, the battle is too fast to
test movement, the arena is too small, and WASD/facing feels incorrect.
Current baseline: sourced Assault Walker hero, CC0 Quaternius enemy, Sentinel
Mech side-pad display, stylized-studs world texture, and CC0 Kenney station
props with a plastic shader. Iteration remains native PC; mobile/web is the
UX/export target.
Accepted: one owned mech, PvE first, semi-auto arena, floating joystick/WASD,
resources -> hangar purchase/craft, shoulder rockets, drone swarm proof, short
dash, `Cooling`, a fixed three-quarter/isometric camera, bright Roblox-like
toy/block world, stylized studs surfaces, and Foundry Warden mini-boss.

## Next Priorities

1. Finish `T0040`: animation/turn/recoil read, slower test battle, larger
   arena, and clearer WASD/facing before more asset sourcing or combat content.
2. Keep reference constraints from
   `gamedesign/projects/mech-builder-battler/references/current_build_mismatch_audit_2026-06-19.md`.
3. Keep latest product evidence in
   `gamedesign/projects/mech-builder-battler/evidence/t0039_hero_motion_attack_juice_2026-06-20.md`.
4. Defer web/mobile export work until explicitly approved; preserve
   mobile-style controls/readability in the PC harness.

# Project Status

## Current Goal

Active game: `Mech Builder Battler` (`mech-builder-battler`): casual
mobile/web-targeted 3D mech builder battler, implemented and iterated first in
the native PC harness.

Project wiki: `gamedesign/projects/mech-builder-battler/README.md`.
Latest control note:
`gamedesign/projects/mech-builder-battler/references/mobile_control_patterns_2026-06-19.md`.

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

`T0021` is done, `T0022` refreshed the mismatch audit, and `T0023` / `T0025` /
`T0026` / `T0027` / `T0028` are in review after asset-first Roblox-like visual
passes. Current baseline: sourced CC-BY Assault Walker hero with whole-body
motion/recoil, stomp rings, weapon charge, target beams, CC0 Quaternius enemy,
studs/world props, and mesh-rendered hero parts instead of shape debug circles.
Iteration remains native PC; mobile/web is the UX/export target.
Accepted: one owned mech, PvE first, semi-auto arena, floating joystick/WASD,
resources -> hangar purchase/craft, shoulder rockets, drone swarm proof, short
dash, `Cooling`, a fixed three-quarter/isometric camera, bright Roblox-like
toy/block world, stylized studs surfaces, and Foundry Warden mini-boss.

## Next Priorities

1. Review `T0028`, then choose the next visual slice: parted limb motion,
   lighting/material polish, or richer Roblox-like world dressing.
2. Keep reference constraints from
   `gamedesign/projects/mech-builder-battler/references/current_build_mismatch_audit_2026-06-19.md`.
3. Keep latest product evidence in
   `gamedesign/projects/mech-builder-battler/evidence/t0028_assault_walker_motion_attack_read_2026-06-20.md`.
4. Defer web/mobile export work until explicitly approved; preserve
   mobile-style controls/readability in the PC harness.

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

`T0021` is done, `T0022` refreshed the reference mismatch audit, and `T0023` /
`T0025` / `T0026` are in review after asset-first Roblox-like visual passes.
Current baseline: downloaded CC0 Quaternius hero mech/enemy, stylized
studs/world props, module-bound attack/movement VFX, brighter toy/plastic hero
atlas, and mesh-rendered hero rails/slots instead of shape debug circles.
Iteration remains native PC; mobile/web is the UX/export target.
Accepted: one owned mech, PvE first, semi-auto arena, floating joystick/WASD,
resources -> hangar purchase/craft, shoulder rockets, drone swarm proof, short
dash, `Cooling`, a fixed three-quarter/isometric camera, bright Roblox-like
toy/block world, stylized studs surfaces, and Foundry Warden mini-boss.

## Next Priorities

1. Review `T0023`-`T0026` evidence and start the next asset-first visual slice:
   source/download a stronger modular or rigged Roblox-like player mech asset
   with clear license/provenance, then replace or kitbash the current hero base.
2. Keep reference constraints from
   `gamedesign/projects/mech-builder-battler/references/current_build_mismatch_audit_2026-06-19.md`.
3. Keep latest product evidence in
   `gamedesign/projects/mech-builder-battler/evidence/t0026_hero_modular_overlay_motion_pass_2026-06-20.md`.
4. Defer web/mobile export work until explicitly approved; preserve
   mobile-style controls/readability in the PC harness.

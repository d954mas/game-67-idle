# Project Status

## Current Goal

Active research concept: `Mech Builder Battler` (`mech-builder-battler`):
casual mobile/web 3D mech builder battler. GDD/spec, references, review,
traceability, mechanics/meta, evidence plan, and fake-shot request are in
review.

Project wiki: `gamedesign/projects/mech-builder-battler/README.md`.
Latest control note: `gamedesign/projects/mech-builder-battler/references/mobile_control_patterns_2026-06-19.md`.

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

Implementation is gated. `T0011`, `T0013`-`T0020` are in review. Exact
UI/economy/combat/final art needs accepted fake shots or stronger evidence.
Implementation/iteration remains native PC; mobile/web is the UX/export target.
Accepted: one owned mech, PvE first, semi-auto arena, floating joystick/WASD,
resources -> hangar purchase/craft, shoulder rockets, drone swarm proof, short
dash, `Cooling` UI label, fixed three-quarter/isometric camera,
landscape-first, industrial salvage sport, Foundry Warden mini-boss.

## Next Priorities

1. Generate/accept fake shots for hangar, battle, reward, and upgrade.
2. Create the first native PC implementation task from the accepted handoff and
   first-slice spec.
3. Fill Mech Arena/CATS/Mechangelion evidence boards before exact
   UI/combat/economy implementation.
4. Build the eventual native PC slice with mobile-style controls/readability;
   defer web/mobile export work until explicitly approved.

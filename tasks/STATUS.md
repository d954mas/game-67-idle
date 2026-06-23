# Project Status

## Current Goal

No active game concept is selected. This repository is a clean AI-first native
game seed, ready for the next game.

The asset/cutout pipeline was reviewed and optimized this iteration (key_matte
default, route_cutout auto-picker, dual_plate; the per-asset edge-color audit
removed; ~4000 lines of legacy/dead code dropped) — see git history.

Mine Cards was a prior pipeline test run; its full game, GDD, and tasks are
preserved in tag `mine-cards-snapshot-2026-06-18` and removed from the working
tree so the next game starts clean. Reusable lessons live in
`AI_PIPELINE_HISTORY.md`.

Ember Road was closed after visual/UX review. Its full game state is preserved
in tag `ember-road-snapshot-2026-06-21`; active tasks were dropped/archive-kept,
and game-specific runtime, assets, project docs, and tools were removed from
the working tree.

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

Capture the user's game concept (references, audience, platform, no-go
constraints), then run the Stage 0 prototype startup path and create a fresh
project wiki plus exactly one scoped task/epic before implementation. Do not
invent a concept.

## Next Priorities

1. Capture the next game concept, then scaffold a fresh project wiki + one
   scoped task/epic before implementation.
2. Keep reusable pipeline/skills/knowledge clean and current between games.
3. Keep native PC scale/focus + visual/teachability/core-loop proof as early
   gates for the next playable UI.

# Project Status

## Current Goal

No active game concept is selected.

Mine Cards was a pipeline test run, not an active game to continue. Its tasks
and epic are closed as `dropped`; keep the evidence as historical input for
future pipeline/skill improvements.

## Current Runtime Surface

Native `game_seed` remains the clean seed work surface:

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
```

Runtime entrypoint: `src/clean_seed_main.c`.

The engine submodule at `external/neotolis-engine` remains read-only from this
repo. Reusable sidecar modules, tools, skills, and game code may be edited here.

## Historical Evidence

- Mine Cards project wiki: `gamedesign/projects/mine-cards/`
- Closed epic: `tasks/epics/E001-mine-cards-v0-01-mining-foundation.md`
- Archived tasks: `tasks/archive/E001/`
- PC UI scale/focus rejection and fix:
  `gamedesign/projects/mine-cards/reviews/t0001_ui_scale_rejection_2026-06-18.md`
- Pipeline lesson:
  `AI_PIPELINE_HISTORY.md`

## Current Gate

Status: `clean seed; improve reusable pipeline/skills before next game`.

Before starting a future game, run the Stage 0 prototype startup path and create
a fresh project wiki/task set. Do not reuse Mine Cards tasks as current work.

## Next Priorities

1. Strengthen reusable skills and pipeline rules from the Mine Cards test run.
2. Keep native PC scale/focus proof as an early gate for future playable UI.
3. Keep task/status context clean until the next game concept is selected.

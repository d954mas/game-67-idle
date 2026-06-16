# Project Status

## Current Goal

No active game concept. The repository is reset to the reusable native
`Game Seed` template.

## Blocking Work

- No current blocker is known.

## Non-blocking Debt

- None recorded.

## Current Gate

Template state only. Start the next prototype with:

```powershell
node tools/game_context/new_prototype.mjs --game-id <game-id> --title "<Title>" --brief "<short brief>"
```

## Required Validation

```powershell
node tools/taskboard/cli.mjs validate
cmake --build build/_cmake/native-debug --target game_seed
```

## Last Known Good Evidence

- Runtime surface: `src/clean_seed_main.c`
- Reusable state/devapi infrastructure: `state/`, `tools/state_codegen/`,
  `src/devapi/`, `tools/devapi/`, `src/game_storage.*`, `external/cjson/`

## Next Priorities

1. Start a fresh prototype wiki/GDD and task set when a new concept is chosen.

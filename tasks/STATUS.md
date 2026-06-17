# Project Status

## Current Goal

Clean project seed for the next game. No active game concept is selected.

The previous Voxelheim prototype has been removed from the working tree. Its
durable process lessons remain in reusable rules, skills, tools, and
`gamedesign/knowledge/`; detailed prototype history remains in git.

## Current Runtime Surface

Native `Game Seed` template:

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
```

Runtime entrypoint: `src/clean_seed_main.c`.

## Reusable Infrastructure Kept

- `state/` schemas, fixtures, migrations, and codegen.
- `src/devapi/`, `tools/devapi/`, and `src/game_storage.*`.
- `tools/game_context/new_prototype.mjs` for starting the next game.
- `tools/product_gate/` visual/product/readability gates.
- `tools/assets/` generated-art and UI asset pipeline tools.
- Reusable design knowledge in `gamedesign/knowledge/`.
- Reusable skills in `.codex/skills/`.

## Start Next Game

Use the startup tool instead of hand-writing the first files:

```powershell
node tools/game_context/new_prototype.mjs --game-id <id> --title "<Name>" --brief "<one sentence>"
node tools/game_context/iteration_context.mjs
node tools/taskboard/cli.mjs validate
```

The startup tool creates:

- `gamedesign/projects/<game-id>/`;
- first active task and epic;
- `reviews/first_slice_visual_gate.md`;
- `visual/live_state_acceptance_matrix.md/json`;
- updated `tasks/STATUS.md` for the new game.

## Current Gate

Do not implement gameplay until a new active concept exists and
`node tools/game_context/iteration_context.mjs` reports the startup gate state.

## Blocking Work

- New game concept not selected yet.

## Next Priorities

1. Pick the next game concept.
2. Run `tools/game_context/new_prototype.mjs`.
3. Fill the first GDD/core-loop/reference/visual gates before runtime work.

# Game Project

AI Studio module for game-folder routing, first-slice startup context, and
playable feature iteration.

## Role

Game Project owns the agent-facing bridge between the reusable AI Studio
harness and a game under `games/<game-id>/`:

- `new_prototype.mjs`: creates the first game design folder, task, epic, and
  startup context pack for a new prototype.
- `iteration_context.mjs`: builds a compact pre-implementation context pack for
  playable game work. Pass `--game-id <id>` when more than one game folder exists.
- `feature_iteration/`: small playable-increment workflow for implementing,
  validating, reviewing, and handing off current-game feature work.
- `state_management/`: schema-first game state workflow and generator for
  `GameState` C APIs, save/load, migrations, and DevAPI state commands.

This module does not own game lore, balance, runtime implementation, GDD
quality rules, asset storage, or task state. Those belong to the current game
folder, `ai_studio/quality/`, `ai_studio/assets/`, and `ai_studio/taskboard/`.
Runtime proof helpers belong to `ai_studio/runtime_automation/`.

## Commands

```powershell
node ai_studio/game_project/new_prototype.mjs --game-id <id> --title <name> --brief <one sentence>
node ai_studio/game_project/iteration_context.mjs --game-id <id> --json-output tmp/prototype_startup_gate_context.json
node --test ai_studio/game_project/game_project.test.mjs
```

## Boundaries

Use this module when starting or orienting the current playable game project.
Use `ai_studio/bootstrap/` when copying the reusable template into a new game
folder. Use `nt-primary-gdd` and `ai_studio/game_design/gdd/` for concept and
GDD content decisions. Use `nt-game-feature-iteration` and
`feature_iteration/` for current-game playable implementation slices. Use
`nt-game-state-management` and `state_management/` for state schema, generated
state API, save/load, migration, and DevAPI state work.

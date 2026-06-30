# Game Project

AI Studio module for active game routing and first-slice startup context.

## Role

Game Project owns the agent-facing bridge between the reusable AI Studio
harness and one current game project:

- `GAME_PROJECT.md`: compact active-game routing file at repository root.
- `new_prototype.mjs`: creates the first game-project wiki, task, epic, and
  startup context pack for a new prototype.
- `iteration_context.mjs`: builds a compact pre-implementation context pack for
  playable game work.

This module does not own game lore, balance, runtime implementation, GDD
quality rules, asset storage, or task state. Those belong to the current game
folder, `ai_studio/quality/`, `ai_studio/assets/`, and `ai_studio/taskboard/`.

## Commands

```powershell
node ai_studio/game_project/new_prototype.mjs --game-id <id> --title <name> --brief <one sentence>
node ai_studio/game_project/iteration_context.mjs --json-output tmp/prototype_startup_gate_context.json
node --test ai_studio/game_project/game_project.test.mjs
```

## Boundaries

Use this module when starting or orienting the current playable game project.
Use `ai_studio/bootstrap/` when copying the reusable template into a new game
folder. Use `nt-primary-gdd` and `ai_studio/game_design/gdd/` for concept and
GDD content decisions.

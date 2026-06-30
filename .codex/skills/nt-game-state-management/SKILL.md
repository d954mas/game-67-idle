---
name: nt-game-state-management
description: Use when adding, changing, testing, reviewing, or migrating game state: schema-first state files, generated C GameState APIs, JSON save/load, versioned migrations, DevAPI state commands, fixtures, inventory/equipment references, or bots/tests that read or write progression data.
---

# NT Game State Management

Use this skill to keep game state explicit, typed, migratable, and
agent-legible.

## Start

1. Read `ai_studio/game_project/state_management/README.md`.
2. Inspect the relevant schema before call sites:
   `state/*.schema.json` in a game project, or `template/state/*.schema.json`
   in this AI Studio repository.
3. Inspect `ai_studio/game_project/state_management/generate_state.py` before
   editing generated state APIs.
4. Change schema/generator/template, then regenerate. Do not hand-edit generated
   `game_state.*` files.

## Commands

```powershell
python ai_studio/game_project/state_management/generate_state.py
python -m unittest ai_studio.game_project.state_management.generate_state_test
```

Use `--schema` and `--out-dir` for game variants or build-local outputs.

## Routing

- For state contracts, documents, save envelopes, DevAPI shape, and domain
  actions, read `ai_studio/game_project/state_management/contract.md`.
- For schema-first change workflow, migrations, generated code, and runtime
  access rules, read `ai_studio/game_project/state_management/workflow.md`.
- For review, read `ai_studio/game_project/state_management/review.md`.
- For runtime DevAPI proof, use `nt-runtime-automation`.
- For playable feature changes that consume state, use `nt-game-feature-iteration`.

## Rules

- Every persisted schema field needs a stable integer `id`; removed fields are
  `reserved`.
- Migrations transform old JSON before parsing into current runtime structs.
- Migrations must not call domain actions.
- Gameplay/UI should use domain actions, not raw `GameState` mutations.
- Expose DevAPI writes only for fields intended to be dev-editable.
- Use logical save keys for normal saves; reserve unsafe paths for explicit
  debug fixtures and migration tests.

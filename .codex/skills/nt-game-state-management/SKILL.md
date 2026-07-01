---
name: nt-game-state-management
description: "Use when adding, changing, testing, reviewing, or migrating game state: schema-first state files, generated C GameState APIs, JSON save/load, versioned migrations, DevAPI state commands, fixtures, inventory/equipment references, or bots/tests that read or write progression data."
---

# NT Game State Management

Use this skill as the agent-facing router for the reusable Game State feature
pack in `features/game-state/`. Keep canonical scripts and references in the
feature pack, not in this skill folder.

## Start

1. Read `features/game-state/README.md`.
2. For install, copy, enable/disable, or template wiring work, read
   `features/game-state/INSTALL.md`.
3. Inspect the relevant schema before call sites:
   `state/*.schema.json` in a game project, or `template/state/*.schema.json`
   in this AI Studio repository.
4. Inspect `features/game-state/scripts/generate_state.py` before
   editing generated state APIs.
5. Change schema/generator/template, then regenerate. Do not hand-edit generated
   `game_state.*` files.

## Commands

```powershell
py -3.12 features/game-state/scripts/generate_state.py
py -3.12 features/game-state/scripts/generate_state_test.py
```

Use `--schema` and `--out-dir` for game variants or build-local outputs.

## Routing

- For state contracts, documents, save envelopes, DevAPI shape, and domain
  actions, read `features/game-state/references/contract.md`.
- For schema-first change workflow, migrations, generated code, and runtime
  access rules, read `features/game-state/references/workflow.md`.
- For review, read `features/game-state/references/review.md`.
- For runtime DevAPI proof, use `nt-runtime-automation`.
- For playable feature changes that consume state, keep schema/state work here;
  use `nt-runtime-automation` for live proof and `nt-quality-checks` for
  acceptance evidence.

## Rules

- Every persisted schema field needs a stable integer `id`; removed fields are
  `reserved`.
- Migrations transform old JSON before parsing into current runtime structs.
- Migrations must not call domain actions.
- Gameplay/UI should use domain actions, not raw `GameState` mutations.
- Expose DevAPI writes only for fields intended to be dev-editable.
- Use logical save keys for normal saves; reserve unsafe paths for explicit
  debug fixtures and migration tests.

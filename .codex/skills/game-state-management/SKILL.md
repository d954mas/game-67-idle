---
name: game-state-management
description: "Use when adding, changing, testing, reviewing, or migrating game state: schema-first state files, generated C GameState APIs, JSON save/load, versioned migrations, DevAPI state commands, fixtures, inventory/equipment references, or bot/test setup that reads or writes progression data."
---

# Game State Management

Use to keep game state explicit, typed, migratable, and agent-legible.

## Load Only What Applies

- `references/state-contract.md`: documents, save envelopes, storage keys,
  DevAPI shape, codegen, dirty/autosave, actions, migration rule.
- `references/state-workflow-rules.md`: schema-first workflow, codegen, native
  validation, DevAPI commands, access pattern, migrations, and `Do not hand-edit`
  generated files.
- `references/state-review-checklist.md`: Review Checklist for migrations,
  generated files, save/load, DevAPI permissions, refs, variants, writes.

## Default Workflow

1. Read `state/*.schema.json` and `tools/state_codegen/generate_state.py` before
   editing call sites.
2. Edit schema/generator/templates, then run
   `py -3.12 tools/state_codegen/generate_state.py`.
3. Put gameplay operations in `game_state_actions.h/.c` or an equivalent domain
   layer.
4. Add deterministic migrations and fixtures for moved/renamed/deleted data.
5. Validate native debug first, then relevant DevAPI scenarios.

## Non-Negotiables

- Do not hand-edit generated `game_state.*` in `src/generated/` or build dirs.
- Do not let archived prototype state fields leak into the clean template.
- Do not use `game_state_actions` in migrations.
- Prefer `map<string,T>` for owned entities and lists of ids for order.
- Every persisted schema field needs a stable `id`; removed fields are
  `reserved`.
- Expose DevAPI writes only for fields marked `devapi: read_write`.
- Use raw `game.state.*` writes for debug/editor/fixtures; prefer semantic
  `game.action.*` for gameplay tests.

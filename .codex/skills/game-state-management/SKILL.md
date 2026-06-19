---
name: game-state-management
description: "Use when adding, changing, testing, reviewing, or migrating game state: schema-first state files, generated C GameState APIs, JSON save/load, versioned migrations, DevAPI state commands, fixtures, inventory/equipment references, or bot/test setup that reads or writes progression data."
---

# Game State Management

Use this skill to keep game state explicit, typed, migratable, and
agent-legible. Treat schemas and generated contracts as the source of truth.

## Load Only What Applies

- `references/state-contract.md`: documents, save envelopes, storage keys,
  DevAPI shape, codegen contract, dirty/autosave, domain actions, and migration
  rule.
- `references/state-workflow-rules.md`: schema-first workflow, codegen, native
  validation, DevAPI commands, access pattern, migrations, and `Do not hand-edit`
  generated files.
- `references/state-review-checklist.md`: Review Checklist for migrations,
  generated files, save/load safety, DevAPI permissions, inventory/equipment
  refs, variant generation, and transactional writes.

## Default Workflow

1. Read `state/*.schema.json` first and inspect
   `tools/state_codegen/generate_state.py` before editing call sites.
2. Edit schema/generator/templates, then regenerate. For default output run
   `py -3.12 tools/state_codegen/generate_state.py`.
3. Put gameplay operations in `game_state_actions.h/.c` or an equivalent domain
   layer.
4. Add deterministic migrations and fixtures for moved, renamed, deleted, or
   compensated data.
5. Validate native first with `cmake --build --preset native-debug` or the
   project native debug preset, then relevant DevAPI scenarios.

## Non-Negotiables

- Do not hand-edit generated `game_state.*` files in `src/generated/` or build
  directories.
- Do not let archived prototype state fields leak into the clean template
  schema or default generated baseline.
- Do not use `game_state_actions` in migrations.
- Prefer `map<string,T>` for owned entities and lists of ids for order.
- Every persisted schema field needs a stable `id`; removed fields are
  `reserved`.
- Expose DevAPI writes only for fields marked `devapi: read_write`.
- Use raw `game.state.*` writes for debug/editor/fixture setup; prefer semantic
  `game.action.*` endpoints for gameplay tests.

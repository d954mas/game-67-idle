---
name: game-state-management
description: "Use when adding, changing, testing, reviewing, or migrating game state: schema-first state files, generated C GameState APIs, JSON save/load, versioned migrations, DevAPI state commands, fixtures, inventory/equipment references, or bot/test setup that reads or writes progression data."
---

# Game State Management

Use this skill to keep game state explicit, typed, migratable, and agent-legible.

## Workflow

1. Read `state/*.schema.json` first. Treat schemas as the source of truth for documents, fields, types, defaults, limits, DevAPI permissions, collections, and migrations.
2. Inspect `tools/state_codegen/generate_state.py` and generated code under `src/generated/` before editing call sites. Do not guess path names.
3. After schema edits, run `py -3.12 tools/state_codegen/generate_state.py`. The generator owns `src/generated/game_state.h`, `src/generated/game_state.c`, `src/generated/game_state_devapi.c`, and embedded schema JSON.
4. Keep runtime code working with the current `GameState` only. Old saves must be transformed before parsing into C structs.
5. Add migrations as `state/migrations/vN_to_vN_plus_1.c`. Migrations receive old `cJSON *state`, mutate it to the next version, and must be deterministic.
6. Add or update fixtures under `state/fixtures/` for every migration that moves, deletes, renames, or compensates data.
7. Put gameplay operations in `game_state_actions.h/.c` or an equivalent domain layer. Gameplay/UI code should call actions, not mutate raw `GameState` fields.
8. Mark successful runtime mutations dirty and autosave from the frame loop, not from each setter call. Web saves should use an explicit browser persistence layer such as `localStorage`, not only `fopen`.
9. Validate on the native PC build first: `cmake --build --preset native-debug` (or the project's native debug preset from `CMakePresets.json`), then relevant `tools/devapi/scenarios/*` scripts. Use WASM only when the user asks or the task targets web behavior.
10. Use DevAPI for runtime checks: `game.state.schema`, `game.state.get`, `game.state.set`, `game.state.patch`, `game.state.save`, `game.state.load`, `game.state.reset`.

## Rules

- Do not hand-edit `src/generated/game_state.h` or `src/generated/game_state_schema.gen.h`; change the schema/generator and regenerate.
- Do not store game-specific state rules inside this skill. Put schema, migrations, fixtures, and tests in the project.
- Do not force all data into one global state. Use separate state documents when lifetime, ownership, save cadence, or migration policy differs.
- Do not make migrations depend on network, wall-clock time, current live balance, mutable external data, or `game_state_actions`.
- Do not use `game_state_actions` in migrations. Migrations operate on old save JSON before current runtime structs and domain actions are valid.
- Do not mutate raw `GameState` from gameplay/UI. Use domain actions for spending resources, inventory changes, tutorial skips, rewards, and other business rules.
- Do not duplicate owned objects. Store item instances once in `items`, then reference them from inventory/equipment by id.
- Prefer `map<string,T>` for owned entities and lists of ids for order. Avoid important references by list index.
- Expose DevAPI writes only for fields marked `devapi: read_write`.
- Make failed state writes transactional: validate a copy, then replace runtime state.
- Clear dirty only after a successful durable save; keep it set or restore it when web sync fails.
- Prefer semantic DevAPI actions such as `game.action.inventory.add_item` for bots and gameplay tests. Use raw `game.state.*` writes for debug, editor overrides, fixtures, and scenario setup.

## Access Pattern

Use this order:

```text
schema -> generated GameState storage -> game_state_actions -> gameplay/UI/semantic DevAPI
```

Generated state stores and serializes data. Domain actions enforce game rules and invariants.

## Migration Pattern

Use declarative moves only for trivial changes. For business logic, write C:

```c
bool game_state_migrate_v2_to_v3(cJSON *state, char *error, int error_cap) {
    /* Remove old items, add compensation, then leave JSON in v3 shape. */
    return true;
}
```

Do not call `game_state_actions` from migrations. If a migration needs game-specific logic, implement it directly against the old JSON with versioned constants stored near that migration.

Migration order is always:

```text
read save JSON -> migrate vN to current -> validate current schema -> parse GameState
```

For detailed command and save contracts, read `references/state-contract.md`.

## Review Checklist

- Reject gameplay/UI direct writes to raw `GameState`; require domain actions.
- Reject `game_state_actions` calls from migrations.
- Check integer/enum parsing rejects fractional JSON numbers.
- Check save envelopes validate both `schema` and `document`.
- Check every persisted schema field has a stable `id`; removed fields are `reserved`.
- Check every string has `max_length`, and every list/map has `max_count`.
- Check saves use a safe write path: parent dirs, temp file, replace.
- Check `game.state.save/load` use logical `key`; use `unsafe_path` only for fixtures and explicit debug.
- Check autosave load/save behavior with a restart scenario, and keep ordinary tests isolated from persisted state.
- Check inventory/equipment references point to existing owned objects.
- Check schema edits are reflected in generated headers, runtime C adapters, DevAPI paths, actions, fixtures, and scenarios.

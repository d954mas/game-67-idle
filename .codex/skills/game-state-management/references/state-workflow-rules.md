# State Workflow Rules

Load this reference when adding or changing state fields, generated APIs,
migrations, runtime actions, DevAPI state commands, fixtures, or save behavior.

## Workflow

1. Read `state/*.schema.json` first. Treat schemas as the source of truth for
   documents, fields, types, defaults, limits, DevAPI permissions, collections,
   and migrations.
2. Inspect `tools/state_codegen/generate_state.py` and selected generated output
   before editing call sites. Do not guess path names.
3. Scalar fields such as bool, int, float, string, enum, and dotted paths are
   generated from schema markers in the source template. Adding a scalar field
   usually needs only a schema edit plus regeneration.
4. Hand-edit templates only for structural patterns such as owned-object maps,
   id lists, and ref-checked optional strings.
5. After default schema edits, run:

```text
py -3.12 tools/state_codegen/generate_state.py
```

The source-tree default output under `src/generated/game_state.*` is the clean
template baseline. CMake presets may generate selected state variants into the
build tree with `--schema` and `--out-dir`.

Closed or archived prototypes must use their own schema file and build-local
generated directory. They must not overwrite clean template generated files.

## Runtime Access Pattern

Use this order:

```text
schema -> generated GameState storage -> game_state_actions -> gameplay/UI/semantic DevAPI
```

Generated state stores and serializes data. Domain actions enforce game rules
and invariants. Gameplay/UI should call `game_state_actions.h/.c` or an
equivalent domain layer, not mutate raw `GameState` fields.

Mark successful runtime mutations dirty and autosave from the frame loop, not
from each setter call. Clear dirty only after a successful durable save; keep it
set or restore it when web sync fails.

## Migration Pattern

Keep runtime code working with the current `GameState` only. Old saves must be
transformed before parsing into C structs.

Add migrations as `state/migrations/vN_to_vN_plus_1.c`. Migrations receive old
`cJSON *state`, mutate it to the next version, and must be deterministic.

```c
bool game_state_migrate_v2_to_v3(cJSON *state, char *error, int error_cap) {
    /* Remove old items, add compensation, then leave JSON in v3 shape. */
    return true;
}
```

Do not call `game_state_actions` from migrations. If a migration needs
game-specific logic, implement it directly against old JSON with versioned
constants stored near the migration.

Migration order is always:

```text
read save JSON -> migrate vN to current -> validate current schema -> parse GameState
```

Add or update fixtures under `state/fixtures/` for every migration that moves,
deletes, renames, or compensates data.

## DevAPI Commands

Use DevAPI for runtime checks:

```text
game.state.schema
game.state.get
game.state.set
game.state.patch
game.state.save
game.state.load
game.state.reset
```

Prefer semantic DevAPI actions such as `game.action.inventory.add_item` for bots
and gameplay tests. Use raw `game.state.*` writes for debug, editor overrides,
fixtures, and scenario setup.

## Rules

- Do not hand-edit generated `game_state.*` files in `src/generated/` or build
  directories; change the schema/generator and regenerate.
- Do not store game-specific state rules inside this skill. Put schema,
  migrations, fixtures, and tests in the project.
- Do not force all data into one global state. Use separate state documents when
  lifetime, ownership, save cadence, or migration policy differs.
- Do not make migrations depend on network, wall-clock time, live balance,
  mutable external data, or `game_state_actions`.
- Do not mutate raw `GameState` from gameplay/UI. Use domain actions for
  spending resources, inventory changes, tutorial skips, rewards, and other
  business rules.
- Do not duplicate owned objects. Store item instances once in `items`, then
  reference them from inventory/equipment by id.
- Prefer `map<string,T>` for owned entities and lists of ids for order. Avoid
  important references by list index.
- Expose DevAPI writes only for fields marked `devapi: read_write`.
- Make failed state writes transactional: validate a copy, then replace runtime
  state.
- Web saves should use an explicit browser persistence layer such as
  `localStorage`, not only `fopen`.
- Normal validation starts on native PC: `cmake --build --preset native-debug`
  or the project's native debug preset from `CMakePresets.json`. Use WASM only
  when the user asks or the task targets web behavior.

For detailed save/document contracts, read `state-contract.md`.

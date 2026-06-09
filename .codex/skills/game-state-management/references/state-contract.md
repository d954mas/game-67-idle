# State Contract

## Documents

Use one state system with multiple documents. Split documents when data has a different lifetime, owner, save cadence, or migration policy.

Common documents:

- `meta`: save index, settings, build/profile metadata, selected slot.
- `player` or `profile`: persistent progression, economy, inventory, tutorial, unlocks.
- `level.<id>`: per-level persistent changes, opened doors, collected pickups, placed objects.
- `run` or `session`: temporary state that may be discarded or checkpointed separately.
- `debug`: dev-only state and automation flags, excluded from release saves unless explicitly allowed.

Each document should have its own:

- schema id
- version
- save envelope
- migration chain
- DevAPI permissions
- fixtures

## Save Slots

Do not create a new schema for each save slot. A slot is a stored copy of one or more state documents.

Example:

```text
saves/
  slot_1/
    player.json
    meta.json
  slot_2/
    player.json
    meta.json
```

`slot_1/player.json` and `slot_2/player.json` use the same schema and document type, but contain different data.

## Save Envelope

```json
{
  "schema": "game_67_idle.player_state",
  "document": "player",
  "version": 3,
  "state": {}
}
```

## DevAPI Shape

Prefer one command family with an explicit document parameter:

```json
{"method":"game.state.schema","params":{"doc":"player"}}
{"method":"game.state.get","params":{"doc":"player","path":"wallet.soft"}}
{"method":"game.state.set","params":{"doc":"level.forest_01","path":"doors.door_03.open","value":true}}
```

The default `doc` may be the main player/profile document for small games. Discover documents from `state/*.schema.json` first. Runtime document discovery should use `game.state.documents` or an equivalent endpoint when the project adds it; do not assume it exists today.

## Codegen Contract

Project schemas in `state/*.schema.json` are the source of truth. Run:

```text
py -3.12 tools/state_codegen/generate_state.py
```

The generator writes generated state files under `src/generated/`, including `game_state.h`, `game_state.c`, `game_state_devapi.c`, and `game_state_schema.gen.h`. The embedded schema JSON is used by `game.state.schema`, so bots, tests, and editors can discover the state contract from the running binary without reading project files.

Schema fields must have stable integer `id` values. Removed fields must move to `reserved` rather than being reused. `list<T>` and `map<string,T>` fields must declare `max_count`; `string` fields must declare `max_length`.

`game_state.c` is emitted from `tools/state_codegen/game_state.c.in` for the currently supported schema shape. If the schema shape grows beyond the supported fields and collections, update the generator and template together.

## Dirty And Autosave

Set dirty only after a mutation validates and reaches the live runtime state. Save from a frame boundary so multiple commands in one frame produce one save attempt.

Clear dirty after the save is durable. On web, `fopen` alone writes only to the in-memory filesystem; prefer saving the serialized state envelope to an explicit browser persistence layer such as `localStorage` for small state documents. If web persistence fails, keep dirty set.

Tests should normally start with a fresh state and autosave disabled. Add a separate restart scenario for autosave persistence.

## Storage Keys

Save/load uses logical keys, not file paths:

```json
{"method":"game.state.save","params":{"key":"autosave","doc":"game"}}
{"method":"game.state.load","params":{"key":"slot_1","doc":"game"}}
```

The storage backend resolves `key + doc`:

```text
native: <root>/<key>/<doc>.json
web:    <namespace>.<key>.<doc>
```

The C storage API uses this order because it matches the physical layout:

```c
game_storage_save_json("autosave", "game", json, error, error_cap);
game_storage_load_json("slot_1", "game", &json, error, error_cap);
```

Use `unsafe_path` only for explicit debug fixtures and migration tests. Normal agents, bots, gameplay tests, and game code should use `key`.

## Migration Rule

Migrate one document at a time. Cross-document migrations are allowed only when explicitly listed in a higher-level migration plan because they need ordering, rollback, and fixtures for every affected document.

Migrations must not call `game_state_actions`. They run before current runtime structs are valid and must transform old JSON directly. If removed content needs compensation, keep versioned compensation tables beside the migration.

## Domain Actions

Gameplay code should use domain actions instead of writing raw fields:

```c
bool game_wallet_spend_soft(GameState *state, int amount, char *error, int error_cap);
bool game_inventory_add_item(GameState *state, const char *def_id, int count, char *error, int error_cap);
bool game_tutorial_skip(GameState *state);
```

Use raw `game.state.set/patch` for debug, editor overrides, fixture setup, and targeted tests. Use semantic `game.action.*` endpoints for bots and end-to-end gameplay checks.

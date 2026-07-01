# State Contract

Load this when deciding state document shape, save/load behavior, DevAPI state
shape, generated state API boundaries, dirty/autosave behavior, migrations, or
domain actions.

## Documents

Use one state system with multiple documents. Split documents when data has a
different lifetime, owner, save cadence, or migration policy.

Common documents:

- `meta`: save index, settings, build/profile metadata, selected slot;
- `player` or `profile`: persistent progression, economy, inventory, tutorial,
  unlocks;
- `level.<id>`: per-level persistent changes;
- `run` or `session`: temporary or checkpointed state;
- `debug`: dev-only state and automation flags.

Each document should have its own schema id, version, save envelope, migration
chain, DevAPI permissions, and fixtures.

## Save Envelope

```json
{
  "schema": "example_game.player_state",
  "document": "player",
  "version": 3,
  "state": {}
}
```

Validate `schema`, `document`, and integer `version` before parsing state.

## Storage Keys

Normal save/load uses logical keys, not file paths:

```json
{"method":"game.state.save","params":{"key":"autosave","doc":"game"}}
{"method":"game.state.load","params":{"key":"slot_1","doc":"game"}}
```

Use `unsafe_path` only for explicit debug fixtures and migration tests.

## Codegen

Project schemas in `state/*.schema.json` are source of truth. The AI Studio
template schema lives in `templates/template/state/game_state.schema.json`.

Run:

```powershell
py -3.12 features/game-state/scripts/generate_state.py
```

The generator writes `game_state.h`, `game_state.c`, `game_state_devapi.c`, and
`game_state_schema.gen.h` into the selected generated directory. The default
template generates those files into the CMake build directory.

Schema fields must have stable integer `id` values. Removed fields must move to
`reserved` rather than being reused. Lists/maps need `max_count`; strings need
`max_length`.

Installed projects should gate runtime state code with `FEATURE_GAME_STATE`.
Generated DevAPI registration is part of the installed feature, but it must be
compiled only when both `FEATURE_GAME_STATE` and `GAME_DEVAPI_ENABLED` are on.

## Dirty And Autosave

Set dirty only after a mutation validates and reaches live runtime state. Save
from a frame boundary so multiple commands in one frame produce one save
attempt. Clear dirty after durable save. If persistence fails, keep dirty set.

## Migrations

Migrate one document at a time. Migrations transform old JSON before parsing
into current C structs. They must not call domain actions or depend on network,
wall-clock time, live balance, or mutable external data.

## Domain Actions

Generated state stores and serializes data. Gameplay rules belong in domain
actions such as wallet, inventory, tutorial, reward, or quest operations.

Use raw `game.state.*` writes for debug/editor overrides, fixture setup, and
targeted tests. Prefer semantic `game.action.*` endpoints for bots and gameplay
checks.

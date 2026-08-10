# Game State Feature

Reusable schema-first game-state feature pack.

The generator and the byte-invariant persistence runtime are owned here and
compiled in place by templates and games. Consumers own only their schemas,
migrations, domain hooks, and composition.

## Purpose

Use this feature when a game needs typed, persistent, DevAPI-visible state that
agents can inspect and update safely.

The feature provides:

- schema-first `GameState` generation;
- stable persisted field paths and `reserved` tombstones;
- generated C storage/serialization and hand-written registry DevAPI dispatch;
- save/load envelope and migration guidance;
- review rules for state changes, fixtures, and runtime proof.

## Contents

```text
features/game-state/
  README.md
  INSTALL.md
  feature.json
  include/
    game_save.h
    game_state_json.h
    game_storage.h
  src/
    game_save.c
    game_save_devapi.c
    game_save_platform_native.c
    game_save_platform_web.c
    game_state_json.c
    game_storage.c
    game_storage_backend.h
    game_storage_backend_native.c
    game_storage_backend_web.c
    game_storage_web.c
  references/
    contract.md
    workflow.md
    review.md
  scripts/
    generate_state.py
    generate_state_test.py
    run_tests.py
    state_modules_test.py
    state_codegen/
  tests/
    items_containers.schema.json
  benchmarks/
    benchmark_codegen.py
    baseline.json
    fixtures/multi_fragment.schema.json
```

## Integration Model

There is no general feature installer. Consumers use this module in place:

1. Compile the reusable runtime from `features/game-state/src/` and expose
   `features/game-state/include/` without copying those files.
2. Keep consumer-owned `state/*.schema.json`, `state/migrations/`, fragment
   registration, domain hooks, and save configuration in the template/game.
3. Generate each fragment's `<id>_state*` files from its local schema into the
   build directory, or
   into a checked-in generated folder if that project explicitly chooses to
   version generated C.
4. Define a unique `GAME_STORAGE_APP_ID` in every consumer.

The default template consumes this module in place. New games inherit only its
CMake wiring and keep using the same root runtime.

For exact install, enable/disable, verification, and uninstall steps, read
`features/game-state/INSTALL.md`.

Default template integration uses:

- schema sources: `templates/template/state/{settings,items,progression,game_state}.schema.json`;
- generated output: `templates/template/build/<config>/generated/game-state/`;
- migrations: `templates/template/state/migrations/`;
- always compiled (the `FEATURE_GAME_STATE` on/off flag was removed
  2026-07-07 — a game without state is impossible);
- DevAPI registrations from the hand-written
  `features/game-state/src/game_save_devapi.c` registry
  dispatch (`game_save_register_devapi(on_change, user)`) only when
  `GAME_DEVAPI_ENABLED` is
  also on;
- semantic runtime commands and domain actions in the game or template source.

For a game-specific variant, pass explicit paths:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state.py --schema games/<game-id>/state/game_state.schema.json --out-dir games/<game-id>/src/generated
```

Runtime state cannot be disabled (no build flag). To remove DevAPI commands
from the build, configure `GAME_DEVAPI_ENABLED=OFF`.

## Commands

Generate from the template schema:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state.py --schema templates/template/state/game_state.schema.json
```

Without `--out-dir`, the command writes to `build/generated/game-state` under
the template or game that owns the required `--schema` path.

Run generator tests:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/run_tests.py
```

The aggregate runner executes these focused suites:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state_test.py
node ai_studio/dev_environment/python_run.mjs -m unittest features/game-state/scripts/state_modules_test.py
node ai_studio/dev_environment/python_run.mjs -m unittest features/game-state/benchmarks/benchmark_codegen_test.py
```

Run the advisory local benchmark:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/benchmarks/benchmark_codegen.py
```

## Boundaries

- The schema is source of truth. Do not hand-edit generated `game_state.*`
  files.
- Runtime feature code always compiles. DevAPI registration is gated by
  `GAME_DEVAPI_ENABLED`; release builds must not compile or register those
  commands.
- `game_save.c` and `game_storage.c` are platform-neutral policy. Exactly one
  save/storage backend is selected by the consumer; Emscripten and DOM access
  stay in the web adapters.
- Generated state stores and serializes data. Gameplay rules belong in domain
  actions owned by the game or template.
- Schema v2 supports one deliberately narrow depth-two `list<Object>`
  aggregate. It generates separate fixed top-level and nested pools and a
  nested JSON projection; it is not a recursive object graph facility.
- Migrations transform old JSON before parsing into current runtime structs.
  They must not call domain actions.
- Raw `game.state.*` writes are for debug/editor overrides, fixtures, and
  targeted tests. Bots and gameplay checks should prefer semantic actions.
- Runtime proof collection belongs to `ai_studio/runtime_automation/`.
- Quality acceptance belongs to `ai_studio/quality/`.
- DevAPI dispatch is compiled from this module only when the consumer enables
  `GAME_DEVAPI_ENABLED`; release builds must keep it disabled.

## Feature-Pack Example Rules

Use this folder as the minimum bar for future feature packs:

- explain what the feature does and what it does not own;
- list dependencies and copy points;
- keep reusable scripts close to the feature;
- keep references specific to the feature;
- expose an agent-facing skill only as a thin router when discoverability helps.

## Public surface

Generated `GameState` files, fragment descriptors, and commands declared in
`feature.json` are public. Generator internals are not.

Version 4 makes New Game transitions explicit: `game_save_new_game()` and
`game_save_apply_pending_new_game()` return `{ state_changed, persisted }`, so
callers always rebind live state when persistence is temporarily unavailable.

Version 3 changes the DevAPI change callback to
`(change, fragment_id, user)`. `fragment_id` is set for `EDIT` and is `NULL`
for full-state `REPLACE`, allowing the game session to reconcile only the
affected domain.

## Validation

Run the `test` command from `feature.json`, then
`node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. Patch preserves the public contract,
minor adds backward-compatible surface, and major permits breaking changes.
Consumers pin both this version and an exact repository revision.

Version `4.2.0` zeroes the alignment gap a repeated section leaves before its record
array. The gap ships inside the payload, so skipping it let two identical emits copy
different bytes into the log. Generated sources change; regenerate. The schema
validator also claims the names the emit body uses as locals and the descriptor
tables it emits, so a field or event named like one of them is rejected instead of
producing C that does not compile.

## Extension points

Extend through game-owned schemas, migrations, hooks, and DevAPI adapters;
game-specific state policy stays outside the generator.

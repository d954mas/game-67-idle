# Game State Install

Game State is an in-place module. Do not copy its runtime into a template or
game. Consumers compile the one source under `features/game-state/` and own
only their schemas, migrations, hooks, and composition.

## Wire the module

Define the module paths in the consumer:

```cmake
set(GAME_STATE_DIR "${GAME_REPO_ROOT}/features/game-state")
set(GAME_STATE_INC "${GAME_STATE_DIR}/include")
set(GAME_STATE_SRC "${GAME_STATE_DIR}/src")
```

Compile the runtime together with the consumer's generated fragments:

```cmake
target_sources(game PRIVATE
    "${GAME_STATE_SRC}/game_state_json.c"
    "${GAME_STATE_SRC}/game_save_writer.c"
    "${GAME_STATE_SRC}/game_storage.c"
    "${GAME_STATE_SRC}/game_save.c")
target_include_directories(game PRIVATE
    "${GAME_STATE_INC}"
    "${GAME_STATE_GENERATED_DIR}")
target_link_libraries(game PRIVATE cjson nt_time nt_log)
```

To enable the allocation-free autosave tick, own the fixed storage budget in
the consumer and configure it before `game_save_init()`:

```c
static char s_save_snapshot[64U * 1024U];

game_save_set_hot_snapshot_buffer(s_save_snapshot, sizeof s_save_snapshot);
game_save_set_live_validator(game_validate_live_save);
game_save_init();
```

The buffer must outlive the runtime. A synchronous storage write consumes it
only after the complete snapshot has been produced. Consumers that do not
configure a buffer retain the legacy JSON autosave path.

The live validator must check every registered fragment without allocation,
and every fragment must provide `write_snapshot`; generated fragments do so
automatically. Registered transforms or retained orphan fragments deliberately
select the legacy JSON path because the bounded writer cannot reproduce those
compatibility transforms byte-for-byte.

The default template deliberately uses `GAME_STORAGE_MAX_BYTES + 1U`: its
reference schema accepts every storage-valid Items state. A production game
should instead set a measured, fixture-proven bound for its own schema.

Select exactly one save platform adapter. Web storage adds its isolated
localStorage adapter:

```cmake
if(EMSCRIPTEN)
    target_sources(game PRIVATE
        "${GAME_STATE_SRC}/game_save_platform_web.c"
        "${GAME_STATE_SRC}/game_storage_backend_web.c"
        "${GAME_STATE_SRC}/game_storage_web.c")
else()
    target_sources(game PRIVATE
        "${GAME_STATE_SRC}/game_save_platform_native.c"
        "${GAME_STATE_SRC}/game_storage_backend_native.c")
endif()
```

When DevAPI is enabled, add the universal registry dispatch:

```cmake
if(GAME_DEVAPI_ENABLED)
    target_sources(game PRIVATE "${GAME_STATE_SRC}/game_save_devapi.c")
endif()
```

The consumer must define a stable, unique `GAME_STORAGE_APP_ID` and its save
policy values:

```cmake
target_compile_definitions(game PRIVATE
    GAME_STORAGE_APP_ID="my-game"
    GAME_SAVE_AUTOSAVE_SLOT="autosave"
    GAME_SAVE_DEBOUNCE_MS=2000
    GAME_SAVE_MAX_INTERVAL_MS=30000
    GAME_SAVE_DOC_VERSION=1)
```

## Consumer-owned files

Keep these in the template/game:

```text
state/*.schema.json
state/migrations/
domain actions and fragment hooks
fragment registration order
save document migrations and validator
CMake generation commands
```

Generate every schema into one build-local directory:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state.py `
  --schema games/<game-id>/state/game_state.schema.json `
  --out-dir games/<game-id>/build/<config>/generated/game-state
```

Register all fragments before `game_save_init()`. Registration order is
deterministic and owned by the consumer.

## Verify

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/run_tests.py
cmake --build <consumer-build> --target game test_game_save
ctest --test-dir <consumer-build> --output-on-failure
node features/validate_contracts.mjs
```

## Remove

Remove the module sources/include path, generated fragment commands, fragment
registrations, and `cjson` link. Runtime state has no soft enable/disable flag.

# Items Core Install

Items Core is an in-place module. Resolve `GAME_REPO_ROOT` once so templates,
public games, and `games/private/<id>` reference the same source directory:

```cmake
set(ITEMS_CORE_DIR     "${GAME_REPO_ROOT}/features/items-core")
set(ITEMS_CORE_INC     "${ITEMS_CORE_DIR}/include")
set(ITEMS_CORE_SRC     "${ITEMS_CORE_DIR}/src")
set(ITEMS_CORE_SCRIPTS "${ITEMS_CORE_DIR}/scripts")
```

## Required game-owned files

```text
<game>/items.lua.json
<game>/design/items/*.lua
<game>/content/items.lock.json
<game>/state/items.schema.json
<game>/src/features/items/reason_tags.h
<game>/src/game_items.c
<game>/src/game_items.h
<game>/src/game_items_devapi.c
```

The manifest allowlists every Lua module. The lock is release compatibility
history; the state schema owns save versioning and hooks. Reason verbs,
game-created containers, owner references, initial grants, and game-specific
migrations remain game code.

## Build-local catalog

Generate the Snapshot and the typed C catalog through the semantic CLI:

```cmake
set(ITEMS_CATALOG_BUILD_DIR "${CMAKE_BINARY_DIR}/generated/items-catalog")
set(ITEMS_CATALOG_SNAPSHOT "${ITEMS_CATALOG_BUILD_DIR}/items.snapshot.json")
set(ITEMS_CATALOG_SOURCE "${ITEMS_CATALOG_BUILD_DIR}/items_catalog.gen.c")

file(GLOB ITEMS_CATALOG_LUA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/design/items/*.lua")
add_custom_command(
    OUTPUT
        "${ITEMS_CATALOG_SNAPSHOT}"
        "${ITEMS_CATALOG_SOURCE}"
        "${ITEMS_CATALOG_BUILD_DIR}/items_catalog.gen.h"
        "${ITEMS_CATALOG_BUILD_DIR}/items_catalog.internal.gen.h"
        "${ITEMS_CATALOG_BUILD_DIR}/items_catalog.luau"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ITEMS_CATALOG_BUILD_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${ITEMS_CORE_SCRIPTS}/items_cli.py"
        --project-root "${CMAKE_CURRENT_SOURCE_DIR}"
        build --out-dir "${ITEMS_CATALOG_BUILD_DIR}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/items.lua.json"
        ${ITEMS_CATALOG_LUA_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/content/items.lock.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/state/items.schema.json"
    VERBATIM)
add_custom_target(items_catalog_gen DEPENDS
    "${ITEMS_CATALOG_SNAPSHOT}"
    "${ITEMS_CATALOG_SOURCE}")
```

Nothing generated here is packed or committed: the catalog is compiled into the
game like any other source.

## Runtime wiring

```cmake
add_dependencies(${GAME_TARGET} items_catalog_gen)
target_sources(${GAME_TARGET} PRIVATE
    "${ITEMS_CORE_SRC}/items_api.c"
    "${ITEMS_CATALOG_SOURCE}"
    "${ITEMS_CORE_SRC}/items_containers.c"
    "${ITEMS_CORE_SRC}/items_reconcile.c"
    src/game_items.c)
target_include_directories(${GAME_TARGET} PRIVATE
    "${ITEMS_CATALOG_BUILD_DIR}"
    "${ITEMS_CORE_INC}"
    "${GAME_EVENTS_INC}"
    "${GAME_STATE_INC}"
    src)
```

`items_containers.c` emits through `game-events` and marks `game-state` dirty,
so both modules and their include directories are required runtime dependencies.

`features/items/items.h` includes the generated `items_catalog.gen.h`, so every
target that compiles a translation unit reaching that header needs
`${ITEMS_CATALOG_BUILD_DIR}` on its include path and a dependency on
`items_catalog_gen`. There is no startup step: the catalog answers from the
first instruction.

When DevAPI is enabled, compile the game-owned `src/game_items_devapi.c` and
register it after `nt_devapi_register_default()`. It exposes bounded
`game.items.container.list` and `game.items.container.inspect` projections.
Do not compile or register this adapter when DevAPI is disabled.

## Tests and release receipt

Use the production Lua catalog in ownership and composition tests: link
`items_api.c` plus `${ITEMS_CATALOG_SOURCE}` exactly as the game target does.

Keep committed catalog validation on ctest:

```cmake
add_test(NAME items_catalog_validate COMMAND "${Python3_EXECUTABLE}"
    "${ITEMS_CORE_SCRIPTS}/items_cli.py"
    --project-root "${CMAKE_CURRENT_SOURCE_DIR}" validate)
```

At a release boundary, after migrations and validation are complete, seal the
compatible storage/level/field history atomically:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> seal-receipt
```

## Verify

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli_test.py
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_c_catalog_test.py
cmake --build templates/template/build/native-debug --target game test_items_api test_items_fragment
ctest --test-dir templates/template/build/native-debug -R "items|progression|template_composition" --output-on-failure
```

To remove the module, remove this CMake wiring and the game-owned Items files.
There is no compatibility flag or fallback catalog path.

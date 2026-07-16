# Items Core Install

Items Core is an in-place module. Templates and games live two levels below
the repository root and reference the same source directory:

```cmake
set(ITEMS_CORE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/../../features/items-core")
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
```

The manifest allowlists every Lua module. The lock is release compatibility
history; the state schema owns save versioning and hooks. Reason verbs,
game-created containers, owner references, initial grants, and game-specific
migrations remain game code.

## Build-local catalog

Generate Snapshot, package, and ABI header through the semantic CLI:

```cmake
set(ITEMS_CATALOG_BUILD_DIR "${CMAKE_BINARY_DIR}/generated/items-catalog")
set(ITEMS_CATALOG_SNAPSHOT "${ITEMS_CATALOG_BUILD_DIR}/items.snapshot.json")
set(ITEMS_CATALOG_PACKAGE "${ITEMS_CATALOG_BUILD_DIR}/items.catalog")
set(ITEMS_CATALOG_ABI_HEADER "${ITEMS_CATALOG_BUILD_DIR}/items_catalog_abi.gen.h")

file(GLOB ITEMS_CATALOG_LUA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/design/items/*.lua")
add_custom_command(
    OUTPUT
        "${ITEMS_CATALOG_SNAPSHOT}"
        "${ITEMS_CATALOG_PACKAGE}"
        "${ITEMS_CATALOG_ABI_HEADER}"
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
    "${ITEMS_CATALOG_PACKAGE}"
    "${ITEMS_CATALOG_ABI_HEADER}")
```

Pack `${ITEMS_CATALOG_PACKAGE}` as the blob asset `items/catalog`. Do not copy
the Snapshot or header into source control.

## Runtime wiring

```cmake
add_dependencies(${GAME_TARGET} items_catalog_gen)
target_sources(${GAME_TARGET} PRIVATE
    "${ITEMS_CORE_SRC}/items_runtime_package.c"
    "${ITEMS_CORE_SRC}/items_runtime_resource.c"
    "${ITEMS_CORE_SRC}/items_containers.c"
    "${ITEMS_CORE_SRC}/items_reconcile.c"
    src/game_items.c)
target_include_directories(${GAME_TARGET} PRIVATE
    "${ITEMS_CATALOG_BUILD_DIR}"
    "${ITEMS_CORE_INC}"
    src)
target_compile_definitions(${GAME_TARGET} PRIVATE
    ITEMS_RUNTIME_PACKAGE_ENABLED=1)
```

After mounting/loading the selected pack, request `items/catalog` as
`NT_ASSET_BLOB`. Once ready, call `items_catalog_try_bind_resource()`. Only a
successful bind may be followed by save load/reconcile, feature initialization,
gameplay, DevAPI state commands, or autosave. Call `items_catalog_shutdown()`
before `nt_resource_shutdown()`.

## Tests and release receipt

Use the production Lua catalog in ownership and composition tests. A small test
loader may read `${ITEMS_CATALOG_PACKAGE}` and call `items_catalog_try_bind()`;
compile those targets with the same ABI header and
`ITEMS_RUNTIME_PACKAGE_ENABLED=1`.

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
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_runtime_package_test.py
cmake --build templates/template/build/native-debug --target game test_items_runtime_package test_items_runtime_resource test_items_fragment
ctest --test-dir templates/template/build/native-debug -R "items|progression|template_composition" --output-on-failure
```

To remove the module, remove this CMake wiring and the game-owned Items files.
There is no compatibility flag or fallback catalog path.

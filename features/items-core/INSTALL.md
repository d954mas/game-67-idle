# Items Core Install

In-place module (precedent `features/game-state`) — no copy step. A consuming
template/game references this module's source and scripts from its own
`CMakeLists.txt` by the depth-2-invariant relative path
`${CMAKE_CURRENT_SOURCE_DIR}/../../features/items-core`, which resolves to the
same repo-root module whether the caller is `templates/template` or
`games/<id>` (both live exactly two levels below the repo root, same as
`ENGINE_DIR` and `GAME_STATE_GENERATOR`).

## CMake wiring

Define the module path variables once (near `ENGINE_DIR`, before the game's
`add_executable`):

```cmake
set(ITEMS_CORE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/../../features/items-core")
set(ITEMS_CORE_INC     "${ITEMS_CORE_DIR}/include")
set(ITEMS_CORE_SRC     "${ITEMS_CORE_DIR}/src")
set(ITEMS_CORE_SCRIPTS "${ITEMS_CORE_DIR}/scripts")
```

Content codegen (writes into the game's OWN generated dir, not the module):

```cmake
add_custom_command(
    OUTPUT "${GAME_SOURCE_GENERATED_DIR}/items_catalog.gen.h" "${GAME_SOURCE_GENERATED_DIR}/items_catalog.gen.c"
    COMMAND "${Python3_EXECUTABLE}" "${ITEMS_CORE_SCRIPTS}/generate_items_catalog.py"
        --catalog "<game>/content/items.json"
        --schema "<game>/content/item_fields.schema.json"
        --out-dir "${GAME_SOURCE_GENERATED_DIR}"
    DEPENDS "<game>/content/items.json" "<game>/content/item_fields.schema.json"
        "${ITEMS_CORE_SCRIPTS}/generate_items_catalog.py")
```

Ownership core (`target_sources`):

```cmake
target_sources(${GAME_TARGET} PRIVATE
    "${ITEMS_CORE_SRC}/items_catalog.c"
    "${ITEMS_CORE_SRC}/items_containers.c"
    "${ITEMS_CORE_SRC}/items_reconcile.c"
    src/features/items/items_bootstrap.c   # game-owned: items_on_new_game() only
)
```

Include path — `ITEMS_CORE_INC` **ahead of** the game's own `src` (so a stray
copy of `items.h` under the game's `src/features/items/` can never shadow the
module; the grep-gate `G-copies` also forbids that copy from existing):

```cmake
target_include_directories(${GAME_TARGET} PRIVATE "${ITEMS_CORE_INC}" src ...)
```

`items_containers.c` calls `items_reason_check()`, declared in the GAME's own
`src/features/items/reason_tags.h` — resolved through the game's `src` on the
include path as part of the game-owned items corner, not shipped by this module
(`G-noleak` grep-gate forbids `reason_tags.h` from existing inside
`features/items-core`).

## Required game-owned files

Every consumer must supply its own:

```text
<game>/content/items.json
<game>/content/item_fields.schema.json
<game>/content/items.lock.json           # destructive-change guard baseline
<game>/state/items.schema.json           # version + hooks: on_new_game/reconcile + migrations
<game>/src/features/items/reason_tags.h  # closed reason-verb list
<game>/src/features/items/items_bootstrap.c  # items_on_new_game() only
```

The items save fragment itself (`items_state.*`, generated) comes from
`features/game-state/scripts/generate_state.py --fragment items` against the
game's `state/items.schema.json` — this module does not generate the fragment,
only the const catalog and the ownership logic that reads/writes it.

## ctest wiring

`items_ops_validate` — the destructive-change guard, run as a ctest target
against the game's REAL committed content. **Every path must be passed
explicitly** (this script's own argparse defaults are script-relative and
resolve inside the module after this move, not the game — R7/H2 of the build
spec):

```cmake
add_test(NAME items_ops_validate COMMAND "${Python3_EXECUTABLE}"
    "${ITEMS_CORE_SCRIPTS}/items_ops.py" validate
    --catalog content/items.json --schema content/item_fields.schema.json
    --baseline content/items.lock.json --state-schema state/items.schema.json
    --src-dir src/features/items
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
```

`--src-dir` points at the GAME's own items corner (`reason_tags.h` +
`items_bootstrap.c`) — the display-name-keying lint only scans that game code;
the ownership core lives (and can be linted separately) inside this module.

`items_ops_test` — self-contained unittest, no game content dependency beyond
the reused `content/item_fields.schema.json`:

```cmake
add_test(NAME items_ops_test COMMAND "${Python3_EXECUTABLE}"
    "${ITEMS_CORE_SCRIPTS}/items_ops_test.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
```

## Verify

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_ops_test.py
ctest --test-dir templates/template/build/native-debug --output-on-failure
```

## Uninstall

No soft (CMake-flag) uninstall. Remove the `ITEMS_CORE_*` CMake wiring, the
`target_sources`/`target_include_directories` entries, the `items_ops_validate`/
`items_ops_test` ctest registrations, and the game-owned files listed above if
no other feature in that game needs them.

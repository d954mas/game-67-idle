# Progression Core Install

In-place module (precedent `features/game-state`, `features/items-core`) — no
copy step. A consuming template/game references this module's source and
scripts from its own `CMakeLists.txt` by the depth-2-invariant relative path
`${CMAKE_CURRENT_SOURCE_DIR}/../../features/progression-core`, which resolves
to the same repo-root module whether the caller is `templates/template` or
`games/<id>` (both live exactly two levels below the repo root, same as
`ENGINE_DIR`/`GAME_STATE_GENERATOR`/`ITEMS_CORE_DIR`).

## Dependency: items-core (L2 -> L1)

`progression.h` includes `features/items/items.h` (the L2->L1 edge) and
`progression.c` reads/spends purse through
`items_purse`/`items_add`/`items_remove`. **Every consumer that installs
`progression-core` must also install `items-core`** and add
`ITEMS_CORE_INC` to its include path (see `features/items-core/INSTALL.md`).
The reverse edge does not exist — items code never mentions progression
(grep-gated, G-rev).

## CMake wiring

Define the module path variables once (near `ENGINE_DIR`/`ITEMS_CORE_*`,
before the game's `add_executable`):

```cmake
set(PROGRESSION_CORE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/../../features/progression-core")
set(PROGRESSION_CORE_INC     "${PROGRESSION_CORE_DIR}/include")
set(PROGRESSION_CORE_SRC     "${PROGRESSION_CORE_DIR}/src")
set(PROGRESSION_CORE_SCRIPTS "${PROGRESSION_CORE_DIR}/scripts")
```

Content codegen (writes into the game's OWN generated dir, not the module;
`--items` cross-checks `currency_def` against the items catalog;
`--state-schema` validates the game-owned progression fragment and supplies
the track-id storage bound):

```cmake
add_custom_command(
    OUTPUT "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.h" "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c"
    COMMAND "${Python3_EXECUTABLE}" "${PROGRESSION_CORE_SCRIPTS}/generate_progression_tracks.py"
        --catalog "<game>/content/progression.json"
        --items "<game>/content/items.json"
        --state-schema "<game>/state/progression.schema.json"
        --out-dir "${GAME_SOURCE_GENERATED_DIR}"
    DEPENDS "<game>/content/progression.json" "<game>/content/items.json"
        "<game>/state/progression.schema.json"
        "${PROGRESSION_CORE_SCRIPTS}/generate_progression_tracks.py")
```

Progression core (`target_sources`):

```cmake
target_sources(${GAME_TARGET} PRIVATE
    "${PROGRESSION_CORE_SRC}/progression.c"
)
```

Include path — `PROGRESSION_CORE_INC` (and `ITEMS_CORE_INC`, the L2->L1
dependency above) **ahead of** the game's own `src` (same M5a rule as
items-core: a stray copy of `progression.h` under the game's
`src/features/progression/` can never shadow the module — and that path no
longer exists after this module extraction):

```cmake
target_include_directories(${GAME_TARGET} PRIVATE
    "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}"
    src ...)
```

## No game-owned C hooks

Unlike items, progression has **no game-side C corner** — no
`src/features/progression/` directory exists in a consuming game
(`src/features/progression/` was deleted entirely by this extraction:
no `reason_tags.h`-equivalent, no `bootstrap.c`-equivalent seed function).
Every consumer still supplies its own:

```text
<game>/content/progression.json           # tracks[] catalog (id/mode/currency_def/max_level/curve)
<game>/content/items.json                 # currency_def cross-check (shared with items-core)
<game>/state/progression.schema.json      # tracks: map<string, {level, xp}>, NO hooks
<game>/src/ui/...                         # composition: reading progression_level()/progression_xp_*() into UI
```

The progression save fragment itself (`progression_state.*`, generated)
comes from `features/game-state/scripts/generate_state.py --fragment
progression` against the game's `state/progression.schema.json` — this
module does not generate the fragment, only the const track/curve tables and
the runtime logic that reads/writes it.

## ctest wiring

`test_progression` — round-trip logic test, links the progression core +
`items-core`'s ownership runtime (progression spends/reads purse) + a
hand-written test track catalog (not the generated one, to avoid a
duplicate-`k_tracks` link conflict with the demo catalog):

```cmake
add_executable(test_progression
    tests/test_progression.c
    tests/test_progression_catalog.c        # hand-written k_tracks (not .gen.c)
    "${PROGRESSION_CORE_SRC}/progression.c"
    "${PROGRESSION_STATE_GENERATED_SOURCE}" "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}"
    "${ITEMS_CORE_SRC}/items_containers.c" "${ITEMS_CORE_SRC}/items_catalog.c" "${ITEMS_CATALOG_GENERATED_SOURCE}"
    "${ITEMS_STATE_GENERATED_SOURCE}" "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
    src/features/items/items_bootstrap.c "${ITEMS_CORE_SRC}/items_reconcile.c"
    src/game_state_json.c "${GAME_EVENTS_SRC}/game_events.c")
target_include_directories(test_progression PRIVATE "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${GAME_EVENTS_INC}" src ...)
```

`test_progression_curve` — golden test over the demo's REAL generated
catalog (`progression_tracks.gen.c`, auto-triggers the content-codegen
custom command). `progression_tracks.gen.h` includes
`features/progression/progression.h`, which includes
`features/items/items.h` — **both** `PROGRESSION_CORE_INC` and
`ITEMS_CORE_INC` must be on this target's include path or the compile fails:

```cmake
add_executable(test_progression_curve
    tests/test_progression_curve.c
    "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c")
target_include_directories(test_progression_curve PRIVATE "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" src "${GAME_SOURCE_GENERATED_DIR}")
```

## Verify

```powershell
ctest --test-dir templates/template/build/native-debug --output-on-failure -R "test_progression|test_progression_curve"
```

## Uninstall

No soft (CMake-flag) uninstall. Remove the `PROGRESSION_CORE_*` CMake
wiring, the `target_sources`/`target_include_directories` entries, the
`test_progression`/`test_progression_curve` ctest registrations, and the
game-owned files listed above if no other feature in that game needs them.
Removing `progression-core` does not require removing `items-core` (the
dependency edge is one-directional, L2 depends on L1, not the reverse).

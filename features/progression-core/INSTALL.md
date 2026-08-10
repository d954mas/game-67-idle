# Progression Core Install

In-place module (precedent `features/game-state`, `features/items-core`) — no
copy step. A consuming template/game references this module's source and
scripts from its own `CMakeLists.txt` via
`${GAME_REPO_ROOT}/features/progression-core`, which works for templates,
public games, and `games/private/<id>`.

## Dependency: items-core (L2 -> L1)

`progression.h` includes `features/items/items.h` (the L2->L1 edge) and
`progression.c` prices, pays, and grants through
`items_can_pay_stacks`/`items_try_pay_stacks`/`items_try_stack_add`/
`items_stack_count`. **Every consumer that installs `progression-core` must
also install `items-core`** and add `ITEMS_CORE_INC` to its include path (see
`features/items-core/INSTALL.md`). The reverse edge does not exist — items code
never mentions progression (grep-gated, G-rev).

## Contract: two reason verbs

The module writes two reasons of its own into items, so a consumer's
game-owned `src/features/items/reason_tags.h` must accept both or every
level-up fails the items verb check:

- `level_cost:<track_id>` — the level's own spend (`manual` on call, `auto` on
  tick).
- `loot:levelup` — the items a reached level grants back. It is a GRANT, not a
  spend; the verb reads that way on purpose.

Both are already in the closed verb list every consumer starts from; the
requirement is only that a game which prunes that list keeps them.

## CMake wiring

Define the module path variables once (near `ENGINE_DIR`/`ITEMS_CORE_*`,
before the game's `add_executable`):

```cmake
set(PROGRESSION_CORE_DIR     "${GAME_REPO_ROOT}/features/progression-core")
set(PROGRESSION_CORE_INC     "${PROGRESSION_CORE_DIR}/include")
set(PROGRESSION_CORE_SRC     "${PROGRESSION_CORE_DIR}/src")
set(PROGRESSION_CORE_SCRIPTS "${PROGRESSION_CORE_DIR}/scripts")
```

Content codegen (writes into the game's OWN generated dir, not the module).
`--snapshot` is the catalog: tracks are authored in the game's Lua beside its
items and ride the Items Snapshot's `tracks` section, so this step must depend
on the snapshot the items codegen produces. `--state-schema` validates the
game-owned progression fragment and supplies the track-id storage bound:

```cmake
add_custom_command(
    OUTPUT "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.h" "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c"
    COMMAND "${Python3_EXECUTABLE}" "${PROGRESSION_CORE_SCRIPTS}/generate_progression_tracks.py"
        --snapshot "<build>/generated/items-catalog/items.snapshot.json"
        --state-schema "<game>/state/progression.schema.json"
        --out-dir "${GAME_SOURCE_GENERATED_DIR}"
    DEPENDS "<build>/generated/items-catalog/items.snapshot.json"
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
    "${GAME_EVENTS_INC}" "${GAME_STATE_INC}"
    src ...)
```

The runtime also includes generated game-state/event headers, calls
`game_save_mark_dirty`, and logs through `nt_log`; install `game-state` and
`game-events` and link the engine logging target used by the host.

## No game-owned C hooks

Unlike items, progression has **no game-side C corner** — no
`src/features/progression/` directory exists in a consuming game
(`src/features/progression/` was deleted entirely by this extraction:
no `reason_tags.h`-equivalent, no `bootstrap.c`-equivalent seed function).
Every consumer still supplies its own:

```text
<game>/design/items/*.lua                 # studio.tracks declarations, listed by the game's items.lua.json
<build>/generated/items-catalog/items.snapshot.json  # the catalog, tracks section included
<game>/state/progression.schema.json      # tracks: map<string, {level, xp}>, NO hooks
<game>/src/features/items/reason_tags.h   # must accept level_cost and loot (see the contract above)
<game>/src/ui/...                         # composition: reading progression_level()/progression_value*() into UI
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
    "${ITEMS_CORE_SRC}/items_containers.c" "${ITEMS_CORE_SRC}/items_api.c" "${ITEMS_CATALOG_SOURCE}"
    "${ITEMS_STATE_GENERATED_SOURCE}" "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
    "${ITEMS_CORE_SRC}/items_reconcile.c"
    "${GAME_STATE_SRC}/game_state_json.c" "${GAME_EVENTS_SRC}/game_events.c")
target_include_directories(test_progression PRIVATE "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${GAME_EVENTS_INC}" "${ITEMS_CATALOG_BUILD_DIR}" src ...)
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

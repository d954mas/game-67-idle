# Python executes on the build host; Emscripten makes target WIN32 false even
# when CMake itself runs on Windows.
if(CMAKE_HOST_WIN32)
    set(STUDIO_PYTHON "${CMAKE_CURRENT_SOURCE_DIR}/../../.venv/Scripts/python.exe")
else()
    set(STUDIO_PYTHON "${CMAKE_CURRENT_SOURCE_DIR}/../../.venv/bin/python")
endif()
if(NOT EXISTS "${STUDIO_PYTHON}")
    message(FATAL_ERROR "Studio Python is missing at ${STUDIO_PYTHON}; run node ai_studio/dev_environment/python_setup.mjs")
endif()
set(Python3_EXECUTABLE "${STUDIO_PYTHON}" CACHE FILEPATH "Studio root venv Python" FORCE)
find_package(Python3 3.12 EXACT COMPONENTS Interpreter REQUIRED)

# И2a: items CONTENT codegen (SECOND codegen, deliberately separate from the
# game-state generator below). Compile-time const
# tables from content/items.json, referenced by the unconditional add_executable
# above (items_catalog.c / items_catalog.gen.c) -- must run after find_package(Python3)
# (code-review H1/#10); textual order vs. that add_executable use does not matter to
# CMake (OUTPUT<->SOURCES path matching is order-independent within a directory).
set(ITEMS_CATALOG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/content/items.json")
set(ITEMS_CATALOG_FIELDS_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/content/item_fields.schema.json")
set(ITEMS_CATALOG_GENERATOR "${ITEMS_CORE_SCRIPTS}/generate_items_catalog.py")
set(ITEMS_CATALOG_GENERATED_HEADER "${GAME_SOURCE_GENERATED_DIR}/items_catalog.gen.h")
set(ITEMS_CATALOG_GENERATED_SOURCE "${GAME_SOURCE_GENERATED_DIR}/items_catalog.gen.c")
add_custom_command(
    OUTPUT
        "${ITEMS_CATALOG_GENERATED_HEADER}"
        "${ITEMS_CATALOG_GENERATED_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_SOURCE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${ITEMS_CATALOG_GENERATOR}"
        --catalog "${ITEMS_CATALOG_JSON}"
        --schema "${ITEMS_CATALOG_FIELDS_SCHEMA}"
        --out-dir "${GAME_SOURCE_GENERATED_DIR}"
    DEPENDS
        "${ITEMS_CATALOG_JSON}"
        "${ITEMS_CATALOG_FIELDS_SCHEMA}"
        "${ITEMS_CATALOG_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating items content catalog (const C tables)"
    VERBATIM
)

# И3a: progression tracks CONTENT codegen (mirrors the items content-codegen
# block above -- deliberately a separate content-codegen invocation, not the
# game-state generator below). Bakes content/progression.json's curve presets
# into compile-time const int64 cost tables; --items cross-checks currency_def
# against the items catalog (ITEMS_CATALOG_JSON, defined just above).
set(PROG_TRACKS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/content/progression.json")
set(PROG_TRACKS_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/progression.schema.json")
set(PROG_TRACKS_GENERATOR "${PROGRESSION_CORE_SCRIPTS}/generate_progression_tracks.py")
add_custom_command(
    OUTPUT
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.h"
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_SOURCE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${PROG_TRACKS_GENERATOR}"
        --catalog "${PROG_TRACKS_JSON}" --items "${ITEMS_CATALOG_JSON}"
        --state-schema "${PROG_TRACKS_STATE_SCHEMA}"
        --out-dir "${GAME_SOURCE_GENERATED_DIR}"
    DEPENDS "${PROG_TRACKS_JSON}" "${ITEMS_CATALOG_JSON}" "${PROG_TRACKS_STATE_SCHEMA}" "${PROG_TRACKS_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating progression tracks catalog (const int64 curve tables)"
    VERBATIM)

set(GAME_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/game_state.schema.json")
set(GAME_STATE_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/generate_state.py")
set(GAME_STATE_GENERATOR_SOURCES
    "${GAME_STATE_GENERATOR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/state_codegen/__init__.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/state_codegen/naming.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/state_codegen/schema.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/state_codegen/model.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/state_codegen/render_state.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/state_codegen/render_events.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/state_codegen/output.py")
set(GAME_STATE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/game-state")
set(GAME_STATE_GENERATED_HEADER "${GAME_STATE_GENERATED_DIR}/game_state.h")
set(GAME_STATE_GENERATED_SOURCE "${GAME_STATE_GENERATED_DIR}/game_state.c")
set(GAME_STATE_GENERATED_SCHEMA "${GAME_STATE_GENERATED_DIR}/game_state_schema.gen.h")
set(GAME_STATE_GENERATED_EVENTS_HEADER "${GAME_STATE_GENERATED_DIR}/game_state_events.gen.h")
set(GAME_STATE_GENERATED_EVENTS_SOURCE "${GAME_STATE_GENERATED_DIR}/game_state_events.gen.c")
add_custom_command(
    OUTPUT
        "${GAME_STATE_GENERATED_HEADER}"
        "${GAME_STATE_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_SCHEMA}"
        "${GAME_STATE_GENERATED_EVENTS_HEADER}"
        "${GAME_STATE_GENERATED_EVENTS_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_STATE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_GENERATOR}"
        --schema "${GAME_STATE_SCHEMA}"
        --out-dir "${GAME_STATE_GENERATED_DIR}"
        --fragment game
    DEPENDS
        "${GAME_STATE_SCHEMA}"
        ${GAME_STATE_GENERATOR_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating installed game-state feature sources"
    VERBATIM
)
# The registry/dispatch/shell/generator are universal over GameSaveFragment,
# so the `settings` fragment needs only its
# own schema generated + wired -- no dispatch or generator edit. Same generated
# dir (already on the include path); make_directory is idempotent and the two
# commands depend on files (not the dir), so parallel ninja is fine.
set(SETTINGS_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/settings.schema.json")
set(SETTINGS_STATE_GENERATED_SOURCE "${GAME_STATE_GENERATED_DIR}/settings_state.c")
add_custom_command(
    OUTPUT
        "${GAME_STATE_GENERATED_DIR}/settings_state.h"
        "${SETTINGS_STATE_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_DIR}/settings_state_schema.gen.h"
        "${GAME_STATE_GENERATED_DIR}/settings_state_events.gen.h"
        "${GAME_STATE_GENERATED_DIR}/settings_state_events.gen.c"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_STATE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_GENERATOR}"
        --schema "${SETTINGS_STATE_SCHEMA}"
        --out-dir "${GAME_STATE_GENERATED_DIR}"
        --fragment settings
    DEPENDS "${SETTINGS_STATE_SCHEMA}" ${GAME_STATE_GENERATOR_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating installed settings-state fragment sources"
    VERBATIM
)
# `items` fragment: owned-map plus a non-empty txn event and
# on_new_game/reconcile hooks.
set(ITEMS_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/items.schema.json")
set(ITEMS_STATE_GENERATED_SOURCE "${GAME_STATE_GENERATED_DIR}/items_state.c")
set(ITEMS_STATE_GENERATED_EVENTS_SOURCE "${GAME_STATE_GENERATED_DIR}/items_state_events.gen.c")
add_custom_command(
    OUTPUT
        "${GAME_STATE_GENERATED_DIR}/items_state.h"
        "${ITEMS_STATE_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_DIR}/items_state_schema.gen.h"
        "${GAME_STATE_GENERATED_DIR}/items_state_events.gen.h"
        "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_STATE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_GENERATOR}"
        --schema "${ITEMS_STATE_SCHEMA}"
        --out-dir "${GAME_STATE_GENERATED_DIR}"
        --fragment items
    DEPENDS "${ITEMS_STATE_SCHEMA}" ${GAME_STATE_GENERATOR_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating installed items-state fragment sources"
    VERBATIM
)
# `progression` fragment: tracks-map (level+xp), non-empty levelup event, and
# no hooks; an empty tracks map means level 0 via lazy allocation.
set(PROGRESSION_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/progression.schema.json")
set(PROGRESSION_STATE_GENERATED_SOURCE "${GAME_STATE_GENERATED_DIR}/progression_state.c")
set(PROGRESSION_STATE_GENERATED_EVENTS_SOURCE "${GAME_STATE_GENERATED_DIR}/progression_state_events.gen.c")
add_custom_command(
    OUTPUT
        "${GAME_STATE_GENERATED_DIR}/progression_state.h"
        "${PROGRESSION_STATE_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_DIR}/progression_state_schema.gen.h"
        "${GAME_STATE_GENERATED_DIR}/progression_state_events.gen.h"
        "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_STATE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_GENERATOR}"
        --schema "${PROGRESSION_STATE_SCHEMA}"
        --out-dir "${GAME_STATE_GENERATED_DIR}"
        --fragment progression
    DEPENDS "${PROGRESSION_STATE_SCHEMA}" ${GAME_STATE_GENERATOR_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating installed progression-state fragment sources"
    VERBATIM
)
target_sources(${GAME_TARGET} PRIVATE
    "${GAME_STATE_GENERATED_SOURCE}"          # includes game_state_fragment descriptor
    "${GAME_STATE_GENERATED_EVENTS_SOURCE}"   # typed event structs/emit/descriptors
    "${SETTINGS_STATE_GENERATED_SOURCE}"      # generated settings fragment state
    src/features/settings/settings.c          # hand-written settings logic
    "${ITEMS_STATE_GENERATED_SOURCE}"         # generated items fragment state
    "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"  # non-empty items.txn event
    src/features/items/items_bootstrap.c       # game-owned on_new_game hook
    "${ITEMS_CORE_SRC}/items_reconcile.c"    # T0337 M1: reconcile/seq-reseed split out of items_bootstrap.c (in-place module)
    "${ITEMS_CORE_SRC}/items_containers.c"   # И2b: ownership/containers/purse (add/remove/move/count/can_afford; T0337 M1: in-place module)
    "${PROGRESSION_STATE_GENERATED_SOURCE}"        # generated progression fragment state
    "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}" # non-empty progression.levelup event
    "${PROGRESSION_CORE_SRC}/progression.c"   # queries/mutations/update over state, items, and tracks
    src/game_state_json.c
    src/game_storage.c
    src/game_save.c
)
target_link_libraries(${GAME_TARGET} PRIVATE cjson)
target_compile_definitions(${GAME_TARGET} PRIVATE
    GAME_SAVE_AUTOSAVE_SLOT="autosave"
    GAME_SAVE_DEBOUNCE_MS=2000
    GAME_SAVE_MAX_INTERVAL_MS=30000
    GAME_SAVE_DOC_VERSION=1
)
target_include_directories(${GAME_TARGET} PRIVATE "${GAME_STATE_GENERATED_DIR}")
if(GAME_DEVAPI_ENABLED)
    # The DevAPI dispatch is a hand-written shell TU (universal over the
    # fragment registry), no longer a generated per-fragment source.
    target_sources(${GAME_TARGET} PRIVATE
        src/game_save_devapi.c
        "${GAME_EVENTS_SRC}/game_events_devapi.c" # event-log tail ring + game.events.tail
        "${GAME_EVENTS_SRC}/game_event_render.c") # descriptor-driven JSON renderer
endif()

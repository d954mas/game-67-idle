# Python executes on the build host; Emscripten makes target WIN32 false even
# when CMake itself runs on Windows.
if(CMAKE_HOST_WIN32)
    set(STUDIO_PYTHON "${GAME_REPO_ROOT}/.venv/Scripts/python.exe")
else()
    set(STUDIO_PYTHON "${GAME_REPO_ROOT}/.venv/bin/python")
endif()
if(NOT EXISTS "${STUDIO_PYTHON}")
    message(FATAL_ERROR "Studio Python is missing at ${STUDIO_PYTHON}; run node ai_studio/dev_environment/python_setup.mjs")
endif()
set(Python3_EXECUTABLE "${STUDIO_PYTHON}" CACHE FILEPATH "Studio root venv Python" FORCE)
find_package(Python3 3.12 EXACT COMPONENTS Interpreter REQUIRED)

# Production Items export. Lua is evaluated once through the semantic CLI,
# which owns receipt validation, Snapshot normalization, and C catalog
# generation. The generated files stay build-local and compile into the game.
set(ITEMS_CATALOG_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/items.lua.json")
file(GLOB ITEMS_CATALOG_LUA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/design/items/*.lua")
set(ITEMS_CATALOG_BUILD_DIR "${CMAKE_BINARY_DIR}/generated/items-catalog")
set(ITEMS_CATALOG_SNAPSHOT "${ITEMS_CATALOG_BUILD_DIR}/items.snapshot.json")
set(ITEMS_CATALOG_SOURCE "${ITEMS_CATALOG_BUILD_DIR}/items_catalog.gen.c")
set(ITEMS_CATALOG_BUILD_SCRIPTS
    "${ITEMS_CORE_SCRIPTS}/items_cli.py"
    "${ITEMS_CORE_SCRIPTS}/items_lua_edit.py"
    "${ITEMS_CORE_SCRIPTS}/items_lua_sandbox.py"
    "${ITEMS_CORE_SCRIPTS}/items_receipt.py"
    "${ITEMS_CORE_SCRIPTS}/items_snapshot.py"
    "${ITEMS_CORE_SCRIPTS}/items_c_identifiers.py"
    "${ITEMS_CORE_SCRIPTS}/items_c_catalog.py")
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
        "${ITEMS_CATALOG_MANIFEST}"
        ${ITEMS_CATALOG_LUA_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/content/items.lock.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/state/items.schema.json"
        ${ITEMS_CATALOG_BUILD_SCRIPTS}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating Items Snapshot and typed C catalog from Lua"
    VERBATIM)
add_custom_target(items_catalog_gen DEPENDS
    "${ITEMS_CATALOG_SNAPSHOT}"
    "${ITEMS_CATALOG_SOURCE}")
add_dependencies(${GAME_TARGET} items_catalog_gen)
target_sources(${GAME_TARGET} PRIVATE
    "${ITEMS_CORE_SRC}/items_api.c"
    "${ITEMS_CATALOG_SOURCE}")
target_include_directories(${GAME_TARGET} PRIVATE "${ITEMS_CATALOG_BUILD_DIR}")

# И3a: progression tracks content codegen is separate from the game-state
# generator below. It bakes content/progression.json's curve presets
# into compile-time const int64 cost tables; --items-snapshot cross-checks
# Progression tracks are authored in the same Lua as items and reach this
# generator through the Snapshot's tracks section; it bakes their per-level
# prices, grants, and column values into compile-time const tables.
set(PROG_TRACKS_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/progression.schema.json")
set(PROG_TRACKS_GENERATOR "${PROGRESSION_CORE_SCRIPTS}/generate_progression_tracks.py")
add_custom_command(
    OUTPUT
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.h"
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_SOURCE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${PROG_TRACKS_GENERATOR}"
        --snapshot "${ITEMS_CATALOG_SNAPSHOT}"
        --state-schema "${PROG_TRACKS_STATE_SCHEMA}"
        --out-dir "${GAME_SOURCE_GENERATED_DIR}"
    DEPENDS "${ITEMS_CATALOG_SNAPSHOT}" "${PROG_TRACKS_STATE_SCHEMA}" "${PROG_TRACKS_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating progression tracks catalog (const baked tables)"
    VERBATIM)

# Every player-visible string is data (content/loc/strings.json) compiled to
# typed accessors. loc_charset.gen.h is an output too: the packed font charset
# follows the corpus, so a translation cannot introduce a glyph the atlas lacks.
set(LOC_STRINGS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/content/loc/strings.json")
set(LOC_GENERATOR "${LOCALIZATION_SCRIPTS}/loc.py")
add_custom_command(
    OUTPUT
        "${GAME_SOURCE_GENERATED_DIR}/loc_strings.gen.h"
        "${GAME_SOURCE_GENERATED_DIR}/loc_strings.gen.c"
        "${GAME_SOURCE_GENERATED_DIR}/loc_keys.gen.json"
        "${GAME_SOURCE_GENERATED_DIR}/loc_charset.gen.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_SOURCE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${LOC_GENERATOR}" generate
        --strings "${LOC_STRINGS_JSON}"
        --out-dir "${GAME_SOURCE_GENERATED_DIR}"
        --source-label "content/loc/strings.json"
    DEPENDS "${LOC_STRINGS_JSON}" "${LOC_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating localization string table"
    VERBATIM)
target_sources(${GAME_TARGET} PRIVATE
    "${LOCALIZATION_SRC}/loc.c"
    "${GAME_SOURCE_GENERATED_DIR}/loc_strings.gen.c")
target_include_directories(${GAME_TARGET} PRIVATE "${LOCALIZATION_INC}")

set(GAME_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/game_state.schema.json")
set(GAME_STATE_GENERATOR "${GAME_REPO_ROOT}/features/game-state/scripts/generate_state.py")
set(GAME_STATE_GENERATOR_SOURCES
    "${GAME_STATE_GENERATOR}"
    "${GAME_REPO_ROOT}/features/game-state/scripts/state_codegen/__init__.py"
    "${GAME_REPO_ROOT}/features/game-state/scripts/state_codegen/naming.py"
    "${GAME_REPO_ROOT}/features/game-state/scripts/state_codegen/schema.py"
    "${GAME_REPO_ROOT}/features/game-state/scripts/state_codegen/model.py"
    "${GAME_REPO_ROOT}/features/game-state/scripts/state_codegen/render_state.py"
    "${GAME_REPO_ROOT}/features/game-state/scripts/state_codegen/render_events.py"
    "${GAME_REPO_ROOT}/features/game-state/scripts/state_codegen/output.py")
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
    WORKING_DIRECTORY "${GAME_REPO_ROOT}"
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
    WORKING_DIRECTORY "${GAME_REPO_ROOT}"
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
    WORKING_DIRECTORY "${GAME_REPO_ROOT}"
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
    WORKING_DIRECTORY "${GAME_REPO_ROOT}"
    COMMENT "Generating installed progression-state fragment sources"
    VERBATIM
)
if(EMSCRIPTEN)
    set(GAME_SAVE_PLATFORM_SOURCES
        "${GAME_STATE_SRC}/game_save_platform_web.c"
        "${GAME_STATE_SRC}/game_storage_backend_web.c"
        "${GAME_STATE_SRC}/game_storage_web.c")
else()
    set(GAME_SAVE_PLATFORM_SOURCES
        "${GAME_STATE_SRC}/game_save_platform_native.c"
        "${GAME_STATE_SRC}/game_storage_backend_native.c")
endif()
target_sources(${GAME_TARGET} PRIVATE
    "${GAME_STATE_GENERATED_SOURCE}"          # includes game_state_fragment descriptor
    "${GAME_STATE_GENERATED_EVENTS_SOURCE}"   # typed event structs/emit/descriptors
    src/game_items.c                           # game-owned container refs and seeding
    "${SETTINGS_STATE_GENERATED_SOURCE}"      # generated settings fragment state
    src/features/settings/settings.c          # hand-written settings logic
    "${ITEMS_STATE_GENERATED_SOURCE}"         # generated items fragment state
    "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"  # non-empty items.txn event
    "${ITEMS_CORE_SRC}/items_reconcile.c"    # T0337 M1: reconcile/seq-reseed split out of items_bootstrap.c (in-place module)
    "${ITEMS_CORE_SRC}/items_containers.c"   # dynamic persistent/ephemeral containers, entries, and bounded inspection
    "${PROGRESSION_STATE_GENERATED_SOURCE}"        # generated progression fragment state
    "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}" # non-empty progression.levelup event
    "${PROGRESSION_CORE_SRC}/progression.c"   # queries/mutations/update over state, items, and tracks
    "${GAME_STATE_SRC}/game_state_json.c"
    "${GAME_STATE_SRC}/game_storage.c"
    "${GAME_STATE_SRC}/game_save.c"
    ${GAME_SAVE_PLATFORM_SOURCES}
)
target_link_libraries(${GAME_TARGET} PRIVATE cjson)
target_compile_definitions(${GAME_TARGET} PRIVATE
    GAME_SAVE_AUTOSAVE_SLOT="autosave"
    GAME_SAVE_DEBOUNCE_MS=2000
    GAME_SAVE_MAX_INTERVAL_MS=30000
    GAME_SAVE_DOC_VERSION=2
)
target_include_directories(${GAME_TARGET} PRIVATE "${GAME_STATE_INC}" "${GAME_STATE_GENERATED_DIR}")
if(GAME_DEVAPI_ENABLED)
    # The DevAPI dispatch is a hand-written shell TU (universal over the
    # fragment registry), no longer a generated per-fragment source.
    target_sources(${GAME_TARGET} PRIVATE
        src/iteration_proof_devapi.c
        src/game_items_devapi.c
        "${GAME_STATE_SRC}/game_save_devapi.c"
        "${GAME_EVENTS_SRC}/game_events_devapi.c" # event-log tail ring + game.events.tail
        "${GAME_EVENTS_SRC}/game_event_render.c") # descriptor-driven JSON renderer
endif()

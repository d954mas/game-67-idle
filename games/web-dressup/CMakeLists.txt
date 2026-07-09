cmake_minimum_required(VERSION 3.25)
project(game LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Two-build convention: the human build (native-debug, VS Code) has no DevAPI;
# agents configure their own dir (build/devapi-debug) with -DGAME_DEVAPI_ENABLED=ON
# and get a distinct window title so both can run side by side.
if(NOT DEFINED GAME_DEVAPI_ENABLED)
    set(GAME_DEVAPI_ENABLED OFF CACHE BOOL "Build the template with engine DevAPI automation support." FORCE)
endif()

set(GAME_PUBLISH_TARGET "local" CACHE STRING "Publish target: local, itch, poki, yandex, or playgama.")
set_property(CACHE GAME_PUBLISH_TARGET PROPERTY STRINGS local itch poki yandex playgama)
if(NOT GAME_PUBLISH_TARGET MATCHES "^(local|itch|poki|yandex|playgama)$")
    message(FATAL_ERROR "GAME_PUBLISH_TARGET must be one of: local, itch, poki, yandex, playgama")
endif()

if(EMSCRIPTEN)
    set(_game_default_em_cache "${CMAKE_SOURCE_DIR}/build/emscripten-cache")
    if(DEFINED ENV{EM_CACHE} AND NOT "$ENV{EM_CACHE}" STREQUAL "")
        set(_game_default_em_cache "$ENV{EM_CACHE}")
    endif()
    set(GAME_EMSCRIPTEN_CACHE_DIR "${_game_default_em_cache}" CACHE PATH "Emscripten cache directory for this game checkout.")
    file(MAKE_DIRECTORY "${GAME_EMSCRIPTEN_CACHE_DIR}")
    set(ENV{EM_CACHE} "${GAME_EMSCRIPTEN_CACHE_DIR}")
    set(_game_emcache_launcher "${CMAKE_COMMAND} -E env EM_CACHE=${GAME_EMSCRIPTEN_CACHE_DIR}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${_game_emcache_launcher}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${_game_emcache_launcher}")
endif()

# Preset name must encode the devapi split: engine libs land in
# build/engine/<preset> and build/tools/builder/<preset>, so two builds sharing
# one preset would overwrite each other's engine libs (nt_input with/without
# inject symbols; nt_devapi_web presence) — the parallel-build collision the
# convention exists to prevent. Every web (publish-target x devapi x build-type)
# gets a UNIQUE name, so no combination can alias into another's engine dir.
# Local keeps the legacy wasm preset names because it is the default developer
# path; portal targets append the publish target.
if(EMSCRIPTEN)
    if(GAME_DEVAPI_ENABLED)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            set(NT_PRESET_NAME "wasm-devapi-release")
        else()
            set(NT_PRESET_NAME "wasm-devapi-debug")
        endif()
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(NT_PRESET_NAME "wasm-release")
    else()
        set(NT_PRESET_NAME "wasm-debug")
    endif()
else()
    if(GAME_DEVAPI_ENABLED)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            set(NT_PRESET_NAME "native-devapi-release")
        else()
            set(NT_PRESET_NAME "devapi-debug")
        endif()
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(NT_PRESET_NAME "native-release")
    else()
        set(NT_PRESET_NAME "native-debug")
    endif()
endif()
if(EMSCRIPTEN AND NOT GAME_PUBLISH_TARGET STREQUAL "local")
    set(NT_PRESET_NAME "${NT_PRESET_NAME}-${GAME_PUBLISH_TARGET}")
endif()
set(GAME_TITLE "Dressup" CACHE STRING "Game window title base")
if(GAME_DEVAPI_ENABLED)
    set(GAME_WINDOW_TITLE "${GAME_TITLE} [AI]")
else()
    set(GAME_WINDOW_TITLE "${GAME_TITLE}")
endif()
if(GAME_DEVAPI_ENABLED)
    set(NT_DEVAPI_ENABLED ON CACHE BOOL "Build the engine DevAPI command layer." FORCE)
    set(NT_UI_DEBUG_TOOLS ON CACHE BOOL "Build UI probe data for engine DevAPI ui.* commands." FORCE)
    set(NT_LOG_RING_ENABLED ON CACHE BOOL "Build log ring data for engine DevAPI obs commands." FORCE)
    set(NT_METRICS_ENABLED ON CACHE BOOL "Build metrics data for engine DevAPI obs commands." FORCE)
    set(NT_INTROSPECT_ENABLED ON CACHE BOOL "Build entity introspection for engine DevAPI obs commands." FORCE)
    set(NT_DEVAPI_GROUP_UI ON CACHE BOOL "Build engine DevAPI ui.* commands." FORCE)
    set(NT_DEVAPI_GROUP_OBS ON CACHE BOOL "Build engine DevAPI log/perf/entity/resource commands." FORCE)
    set(NT_DEVAPI_GROUP_CAPTURE ON CACHE BOOL "Build engine DevAPI capture.frame/capture.region commands." FORCE)
else()
    set(NT_DEVAPI_ENABLED OFF CACHE BOOL "Build the engine DevAPI command layer." FORCE)
    foreach(_grp CORE TIME INPUT DISCOVERY UI OBS ENTITY_WRITE CAPTURE)
        set(NT_DEVAPI_GROUP_${_grp} OFF CACHE BOOL "Disabled with GAME_DEVAPI_ENABLED=OFF." FORCE)
    endforeach()
endif()

# E4: GAME_ANALYTICS_ENABLED gates the local analytics writer. A -D override wins (a cache
# entry -> DEFINED -> persists between configures); otherwise the default = GAME_DEVAPI_ENABLED
# as a PLAIN variable (recomputed EVERY configure, so flipping GAME_DEVAPI_ENABLED in a live
# tree updates it -- a CACHE...FORCE default would go stale). NOT CACHE on the default path.
if(NOT DEFINED GAME_ANALYTICS_ENABLED)
    set(GAME_ANALYTICS_ENABLED ${GAME_DEVAPI_ENABLED})
endif()
if(NOT DEFINED GAME_EVENTS_LOG_MIRROR)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(GAME_EVENTS_LOG_MIRROR OFF)
    else()
        set(GAME_EVENTS_LOG_MIRROR ON)
    endif()
endif()

if(GAME_PUBLISH_TARGET STREQUAL "local")
    set(GAME_PLATFORM_SDK "mock")
    set(GAME_PLATFORM_TARGET_ID 0)
    set(GAME_PLATFORM_SDK_ID 0)
    set(GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED 1)
    set(GAME_PLATFORM_ADS_SUPPORTED 1)
    set(GAME_PLATFORM_REWARDED_SUPPORTED 1)
    set(GAME_PLATFORM_STORAGE_SUPPORTED 1)
elseif(GAME_PUBLISH_TARGET STREQUAL "itch")
    set(GAME_PLATFORM_SDK "mock")
    set(GAME_PLATFORM_TARGET_ID 1)
    set(GAME_PLATFORM_SDK_ID 0)
    set(GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED 1)
    set(GAME_PLATFORM_ADS_SUPPORTED 0)
    set(GAME_PLATFORM_REWARDED_SUPPORTED 0)
    set(GAME_PLATFORM_STORAGE_SUPPORTED 0)
elseif(GAME_PUBLISH_TARGET STREQUAL "poki")
    set(GAME_PLATFORM_SDK "poki")
    set(GAME_PLATFORM_TARGET_ID 2)
    set(GAME_PLATFORM_SDK_ID 1)
    set(GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED 0)
    set(GAME_PLATFORM_ADS_SUPPORTED 1)
    set(GAME_PLATFORM_REWARDED_SUPPORTED 1)
    set(GAME_PLATFORM_STORAGE_SUPPORTED 0)
elseif(GAME_PUBLISH_TARGET STREQUAL "yandex")
    set(GAME_PLATFORM_SDK "yandex")
    set(GAME_PLATFORM_TARGET_ID 3)
    set(GAME_PLATFORM_SDK_ID 2)
    set(GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED 0)
    set(GAME_PLATFORM_ADS_SUPPORTED 1)
    set(GAME_PLATFORM_REWARDED_SUPPORTED 1)
    set(GAME_PLATFORM_STORAGE_SUPPORTED 1)
elseif(GAME_PUBLISH_TARGET STREQUAL "playgama")
    set(GAME_PLATFORM_SDK "playgama")
    set(GAME_PLATFORM_TARGET_ID 4)
    set(GAME_PLATFORM_SDK_ID 3)
    set(GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED 0)
    set(GAME_PLATFORM_ADS_SUPPORTED 1)
    set(GAME_PLATFORM_REWARDED_SUPPORTED 1)
    set(GAME_PLATFORM_STORAGE_SUPPORTED 1)
endif()
if(NOT DEFINED GAME_PLATFORM_SDK_DEBUG_UI)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(GAME_PLATFORM_SDK_DEBUG_UI OFF)
    else()
        set(GAME_PLATFORM_SDK_DEBUG_UI ON)
    endif()
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND GAME_PLATFORM_SDK_DEBUG_UI)
    message(FATAL_ERROR "GAME_PLATFORM_SDK_DEBUG_UI must be OFF for Release artifacts.")
endif()

# Shared engine (one copy, referenced not copied). Templates and games live one
# folder below the repo groups, so the engine is two levels up.
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../external/neotolis-engine")
add_subdirectory("${ENGINE_DIR}" "${CMAKE_BINARY_DIR}/_engine")
if(NOT EMSCRIPTEN AND NOT TARGET nt_builder)
    add_subdirectory("${ENGINE_DIR}/tools/builder" "${CMAKE_BINARY_DIR}/_builder")
    # The pack builder is an offline tool: force -O2 even in Debug game trees
    # (basisu at -O0 encodes for minutes, not seconds). Engine CI does the same
    # by building its pack builders with a release preset; games get the same
    # effect here without patching the engine. Asserts stay on (no NDEBUG).
    foreach(nt_builder_opt_tgt nt_basisu_encoder clipper2 nt_clipper2_bridge tinycthread nt_builder builder)
        target_compile_options(${nt_builder_opt_tgt} PRIVATE $<$<NOT:$<CONFIG:Release>>:-O2>)
    endforeach()
endif()

# T0337: in-place modules (precedent features/game-state). Depth-2 invariant:
# ../../ == repo root for BOTH templates/template and games/<id>, so these paths
# survive games/new_game.mjs verbatim (like ENGINE_DIR :80 / GAME_STATE_GENERATOR :210).
set(ITEMS_CORE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/items-core")
set(ITEMS_CORE_INC        "${ITEMS_CORE_DIR}/include")
set(ITEMS_CORE_SRC        "${ITEMS_CORE_DIR}/src")
set(ITEMS_CORE_SCRIPTS    "${ITEMS_CORE_DIR}/scripts")
# T0337 M2: progression-core (L2 -> items-core L1, §2.4). Same depth-2 invariant as above.
set(PROGRESSION_CORE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/../../features/progression-core")
set(PROGRESSION_CORE_INC     "${PROGRESSION_CORE_DIR}/include")
set(PROGRESSION_CORE_SRC     "${PROGRESSION_CORE_DIR}/src")
set(PROGRESSION_CORE_SCRIPTS "${PROGRESSION_CORE_DIR}/scripts")
set(PLATFORM_SDK_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/../../features/platform-sdk")
set(PLATFORM_SDK_INC         "${PLATFORM_SDK_DIR}/include")
set(PLATFORM_SDK_SRC         "${PLATFORM_SDK_DIR}/src")
set(PLATFORM_SDK_WEB         "${PLATFORM_SDK_DIR}/web")
set(GAME_EVENTS_DIR          "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-events")
set(GAME_EVENTS_INC          "${GAME_EVENTS_DIR}/include")
set(GAME_EVENTS_SRC          "${GAME_EVENTS_DIR}/src")

set(GAME_TARGET game)
set(GAME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(GAME_PACK_DIR "${CMAKE_BINARY_DIR}/pack")
set(GAME_ASSETS_DIR "${GAME_OUTPUT_DIR}/assets")
set(GAME_SOURCE_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")

# --- asset pack builder (runs at build time -> game.ntpack + asset-id header) ---
if(NOT EMSCRIPTEN)
    add_executable(build_game_packs src/build_packs.c)
    target_link_libraries(build_game_packs PRIVATE nt_builder nt_log)
    target_compile_definitions(build_game_packs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(build_game_packs PRIVATE -U_DLL)
    set_target_properties(build_game_packs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}")

    add_custom_command(
        OUTPUT "${GAME_PACK_DIR}/game.ntpack" "${CMAKE_CURRENT_SOURCE_DIR}/src/generated/game_assets.h"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_PACK_DIR}"
        COMMAND $<TARGET_FILE:build_game_packs> "${GAME_PACK_DIR}"
        DEPENDS build_game_packs src/build_packs.c
            assets/shaders/slug_text.vert assets/shaders/slug_text.frag
            assets/shaders/mesh_inst.vert assets/shaders/mesh_inst.frag
            assets/shaders/mesh_tex.vert assets/shaders/mesh_tex.frag
            assets/shaders/sprite.vert assets/shaders/sprite.frag
            assets/ui/panel.png assets/ui/button.png
            assets/ui/slider_track.png assets/ui/slider_fill.png assets/ui/slider_thumb.png
            assets/icons/gold.png assets/icons/xp.png assets/icons/energy.png
            assets/icons/potion.png assets/icons/sword.png assets/icons/wood.png
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building game asset pack"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${GAME_ASSETS_DIR}/game.ntpack"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_ASSETS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GAME_PACK_DIR}/game.ntpack" "${GAME_ASSETS_DIR}/game.ntpack"
        DEPENDS "${GAME_PACK_DIR}/game.ntpack"
        COMMENT "Copying game.ntpack -> bin/assets/"
        VERBATIM
    )
    add_custom_target(game_asset_packs DEPENDS "${GAME_ASSETS_DIR}/game.ntpack")
endif()

# --- game executable (the conductor + systems) ---
add_executable(${GAME_TARGET}
    src/main.c
    src/platform_lifecycle.c
    "${GAME_EVENTS_SRC}/game_events.c" # E1
    src/game_log.c               # E4.B: built-in "log" event type (unconditional leaf)
    src/features/game_features.c # E1
    src/features/dress_room/dress_room_state.c # M0: outfit catalog + equip state
    src/features/dress_room/dress_room_ui.c    # M0: Dress Room dual-layout UI
    src/ui/hud.c
    src/ui/platform_sdk_debug.c
    src/ui/ui_runtime.c
    src/ui/theme.c
    src/render/render_mesh.c
    src/render/capture.c
    src/systems/sys_move.c
    src/features/settings/settings_screen.c
    "${ITEMS_CORE_SRC}/items_catalog.c"        # И2a: catalog-lookup over the codegen tables below (T0337 M1: in-place module)
    "${GAME_SOURCE_GENERATED_DIR}/items_catalog.gen.c" # И2a: const item/container tables (content codegen)
    "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c" # И3a: const progression track/curve tables (content codegen)
    src/game_format.c                                # И3b: L0 int64-abbrev formatter
    "${PLATFORM_SDK_SRC}/platform_sdk.c"             # T0339: target/sdk identity + policy capabilities
    src/features/resource_panel/resource_panel.c     # И3b: generic counter/bar widget (zero feature headers)
    src/ui/demo_hud.c                                # kept for template tests; not drawn in M0
)
if(NOT EMSCRIPTEN)
    add_dependencies(${GAME_TARGET} game_asset_packs)
endif()
# Game-state provisioning is always on (lead decision 2026-07-07: "a game without
# state is impossible"); the former on/off CMake axis is gone (И2-0).
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# И2a: items CONTENT codegen (SECOND codegen, deliberately separate from the
# game-state generator below -- design doc §9 "не смешивать"). Compile-time const
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
set(PROG_TRACKS_GENERATOR "${PROGRESSION_CORE_SCRIPTS}/generate_progression_tracks.py")
add_custom_command(
    OUTPUT
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.h"
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_SOURCE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${PROG_TRACKS_GENERATOR}"
        --catalog "${PROG_TRACKS_JSON}" --items "${ITEMS_CATALOG_JSON}"
        --out-dir "${GAME_SOURCE_GENERATED_DIR}"
    DEPENDS "${PROG_TRACKS_JSON}" "${ITEMS_CATALOG_JSON}" "${PROG_TRACKS_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating progression tracks catalog (const int64 curve tables)"
    VERBATIM)

set(GAME_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/game_state.schema.json")
set(GAME_STATE_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/generate_state.py")
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
        "${GAME_STATE_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating installed game-state feature sources"
    VERBATIM
)
# A6: SECOND fragment. The registry/dispatch/shell/generator are all universal
# over GameSaveFragment, so a second live fragment `settings` needs only its
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
    DEPENDS "${SETTINGS_STATE_SCHEMA}" "${GAME_STATE_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating installed settings-state fragment sources"
    VERBATIM
)
# И2a: THIRD fragment `items` -- owned-map (plain ownership table, §2.2) + txn
# event (NOT empty, unlike settings -- R2) + hooks (on_new_game/reconcile, Р9).
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
    DEPENDS "${ITEMS_STATE_SCHEMA}" "${GAME_STATE_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating installed items-state fragment sources"
    VERBATIM
)
# И3a: FOURTH fragment `progression` -- tracks-map (level+xp, same flat-map
# shape as items' owned, §2.1) + levelup event (NOT empty) + NO hooks (§5.3
# OQ3: fresh game = empty tracks = level 0 via lazy allocation; no bootstrap.c).
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
    DEPENDS "${PROGRESSION_STATE_SCHEMA}" "${GAME_STATE_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating installed progression-state fragment sources"
    VERBATIM
)
target_sources(${GAME_TARGET} PRIVATE
    "${GAME_STATE_GENERATED_SOURCE}"   # A4: also carries the generated game_state_fragment descriptor
    "${GAME_STATE_GENERATED_EVENTS_SOURCE}"   # E2: typed event structs/emit/descriptors
    "${SETTINGS_STATE_GENERATED_SOURCE}"   # A6: generated settings fragment state layer
    src/features/settings/settings.c       # A6: hand-written settings logic (Р9)
    "${ITEMS_STATE_GENERATED_SOURCE}"        # И2a: generated items fragment state layer
    "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}" # И2a: items.txn event (R2: not empty, must link)
    src/features/items/items_bootstrap.c     # И2a stubs -> И2b real bodies (Р9 §7.3: on_new_game only, T0337 M1 split)
    "${ITEMS_CORE_SRC}/items_reconcile.c"    # T0337 M1: reconcile/seq-reseed split out of items_bootstrap.c (in-place module)
    "${ITEMS_CORE_SRC}/items_containers.c"   # И2b: ownership/containers/purse (add/remove/move/count/can_afford; T0337 M1: in-place module)
    "${PROGRESSION_STATE_GENERATED_SOURCE}"        # И3a: generated progression fragment state layer
    "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}" # И3a: progression.levelup event (not empty, must link)
    "${PROGRESSION_CORE_SRC}/progression.c"   # И3a: queries/mutations/update (uses progression_state + items + tracks; T0337 M2: in-place module)
    src/game_state_json.c
    src/game_storage.c
    src/game_save.c              # A3
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
    # A5: the DevAPI dispatch is a hand-written shell TU (universal over the
    # fragment registry), no longer a generated per-fragment source.
    target_sources(${GAME_TARGET} PRIVATE
        src/game_save_devapi.c
        "${GAME_EVENTS_SRC}/game_events_devapi.c" # E3: event-log tail ring + game.events.tail
        "${GAME_EVENTS_SRC}/game_event_render.c") # E3: descriptor-driven JSON renderer
endif()
# E4: the E3 renderer is ALSO the analytics writer's dependency. When devapi is OFF but
# analytics is ON, pull it in here -- guarded against the devapi block's copy above so it
# is never listed twice (E3 lines untouched; the renderer compiles if EITHER wants it).
if((GAME_ANALYTICS_ENABLED OR GAME_EVENTS_LOG_MIRROR) AND NOT GAME_DEVAPI_ENABLED)
    target_sources(${GAME_TARGET} PRIVATE "${GAME_EVENTS_SRC}/game_event_render.c") # E3 renderer (shared with E4/mirror)
endif()
if(GAME_ANALYTICS_ENABLED)
    target_sources(${GAME_TARGET} PRIVATE "${GAME_EVENTS_SRC}/game_analytics.c") # E4: local analytics writer
endif()
if(GAME_EVENTS_LOG_MIRROR)
    target_sources(${GAME_TARGET} PRIVATE "${GAME_EVENTS_SRC}/game_events_log_mirror.c")
endif()
target_include_directories(${GAME_TARGET} PRIVATE
    "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${PLATFORM_SDK_INC}" "${GAME_EVENTS_INC}"
    src "${GAME_SOURCE_GENERATED_DIR}" "${ENGINE_DIR}/deps/glfw/deps")
target_compile_definitions(${GAME_TARGET} PRIVATE GAME_WINDOW_TITLE="${GAME_WINDOW_TITLE}")
target_link_libraries(${GAME_TARGET} PRIVATE
    nt_core nt_app nt_input nt_window nt_gfx nt_text_renderer nt_font
    nt_resource nt_material nt_shared nt_render nt_hash nt_fs nt_log nt_math nt_time
    nt_mesh_renderer nt_entity nt_transform_comp nt_mesh_comp nt_material_comp nt_drawable_comp
    nt_ui nt_sprite_renderer nt_atlas nt_mem_scratch nt_clipboard
)
if(EMSCRIPTEN)
    target_sources(${GAME_TARGET} PRIVATE "${PLATFORM_SDK_SRC}/platform_sdk_web.c")
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http nt_platform_web)
    # Web-devapi host contract (nt_devapi_web.h): the host exports the JS
    # transport entry points; the exports are also what pulls the EM_JS object
    # out of libnt_devapi_web.a so nt_devapi_web_install_shim resolves at link.
    set(GAME_WEB_EXPORTS "_main,_malloc,_free,_platform_sdk_web_complete_init,_platform_sdk_web_complete_interstitial,_platform_sdk_web_complete_rewarded")
    if(GAME_DEVAPI_ENABLED)
        set(GAME_WEB_EXPORTS "${GAME_WEB_EXPORTS},_nt_devapi_web_submit,_nt_devapi_web_poll")
    endif()
    target_link_options(${GAME_TARGET} PRIVATE
        "SHELL:-sFORCE_FILESYSTEM=1"
        "SHELL:-sEXPORTED_FUNCTIONS=${GAME_WEB_EXPORTS}"
        "SHELL:-sMIN_WEBGL_VERSION=2"
        "SHELL:-sMAX_WEBGL_VERSION=2"
        "SHELL:-sFULL_ES3=1"
        # Engine targets set memory flags PRIVATE on static libs, which never
        # reach the executable link: without these the heap is fixed at 16MB
        # and texture transcode OOMs (Aborted(OOM) on web).
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sINITIAL_MEMORY=64MB"
    )
    if(GAME_DEVAPI_ENABLED)
        # The engine JS shim drives the transport through Module.ccall, which
        # recent emcc no longer exports by default (runtime need, not link).
        target_link_options(${GAME_TARGET} PRIVATE
            "SHELL:-sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString")
    endif()
else()
    # glad/stb_image_write are native-only (no GL loader / file dump on web).
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http_stub glad stb_image_write)
endif()
if(EMSCRIPTEN)
    set(GAME_PLATFORM_SDK_RELEASE_JS "false")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(GAME_PLATFORM_SDK_RELEASE_JS "true")
    endif()

    file(MAKE_DIRECTORY "${GAME_OUTPUT_DIR}")
    configure_file("${PLATFORM_SDK_WEB}/platform-sdk.js"
                   "${GAME_OUTPUT_DIR}/platform-sdk.js" COPYONLY)
    configure_file("${PLATFORM_SDK_WEB}/platform-sdk-core.js"
                   "${GAME_OUTPUT_DIR}/platform-sdk-core.js" COPYONLY)
    configure_file("${PLATFORM_SDK_WEB}/adapters/${GAME_PLATFORM_SDK}.js"
                   "${GAME_OUTPUT_DIR}/platform-sdk-adapter.js" COPYONLY)
    file(REMOVE "${GAME_OUTPUT_DIR}/platform-sdk-debug-ui.js")
    if(GAME_PUBLISH_TARGET STREQUAL "playgama")
        configure_file("${PLATFORM_SDK_WEB}/portal/playgama-bridge-config.json"
                       "${GAME_OUTPUT_DIR}/playgama-bridge-config.json" COPYONLY)
    else()
        file(REMOVE "${GAME_OUTPUT_DIR}/playgama-bridge-config.json")
    endif()

    add_custom_target(platform_sdk_web_assets
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/platform-sdk.js" "${GAME_OUTPUT_DIR}/platform-sdk.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/platform-sdk-core.js" "${GAME_OUTPUT_DIR}/platform-sdk-core.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/adapters/${GAME_PLATFORM_SDK}.js" "${GAME_OUTPUT_DIR}/platform-sdk-adapter.js"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${GAME_OUTPUT_DIR}/platform-sdk-debug-ui.js"
        DEPENDS
            "${PLATFORM_SDK_WEB}/platform-sdk.js"
            "${PLATFORM_SDK_WEB}/platform-sdk-core.js"
            "${PLATFORM_SDK_WEB}/adapters/${GAME_PLATFORM_SDK}.js"
        COMMENT "Staging platform-sdk web backend assets (${GAME_PUBLISH_TARGET} -> ${GAME_PLATFORM_SDK})"
        VERBATIM)
    if(GAME_PUBLISH_TARGET STREQUAL "playgama")
        add_custom_target(platform_sdk_playgama_config_asset
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/portal/playgama-bridge-config.json"
                "${GAME_OUTPUT_DIR}/playgama-bridge-config.json"
            DEPENDS "${PLATFORM_SDK_WEB}/portal/playgama-bridge-config.json"
            COMMENT "Staging Playgama Bridge placeholder config"
            VERBATIM)
        add_dependencies(platform_sdk_web_assets platform_sdk_playgama_config_asset)
    endif()

    # Deliver the tracked web shell to bin/ at configure time. Two substitutions:
    # the tab title (GAME_TITLE) and Module.arguments. The engine web devapi shim
    # installs only if main() saw `--devapi <port>` in argv (main.c parses it, and
    # on web argv comes ONLY from Module.arguments); the devapi presets carry the
    # flag, the human presets (devapi OFF) expand to empty.
    if(GAME_DEVAPI_ENABLED)
        set(GAME_DEVAPI_MODULE_ARGS "arguments: ['--devapi', '17890'],")
    else()
        set(GAME_DEVAPI_MODULE_ARGS "")
    endif()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/web/index.html.in"
                   "${GAME_OUTPUT_DIR}/index.html" @ONLY)
endif()
if(GAME_DEVAPI_ENABLED)
    # E3: readable event type/hash labels for game.events.tail. Consuming-CMake edit
    # (engine tree untouched); devapi presets only -> no collision with native-debug's
    # clean nt_hash (each preset has its own build/engine/<preset> tree).
    target_compile_definitions(nt_hash PRIVATE NT_HASH_LABELS=1)
    if(EMSCRIPTEN)
        set(GAME_DEVAPI_TRANSPORT nt_devapi_web nt_platform_web)
    else()
        set(GAME_DEVAPI_TRANSPORT nt_devapi_net)
    endif()
    target_link_libraries(${GAME_TARGET} PRIVATE
        ${GAME_DEVAPI_TRANSPORT}
        nt_devapi_default
        nt_platform
        nt_metrics
        nt_log_ring
    )
endif()
if(WIN32 AND NOT EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE winmm)
endif()
# On web the engine streams packs over HTTP relative to the page URL, so the
# path must be RELATIVE and the pack ships as a plain file next to index.html
# (tools/build_web.sh copies it from the native build). Native keeps the
# absolute bin/assets path.
if(EMSCRIPTEN)
    set(GAME_PACK_RUNTIME_PATH "assets/game.ntpack")
else()
    set(GAME_PACK_RUNTIME_PATH "${GAME_ASSETS_DIR}/game.ntpack")
endif()
target_compile_definitions(${GAME_TARGET} PRIVATE
    GAME_ASSET_PACK_PATH="${GAME_PACK_RUNTIME_PATH}"
    PLATFORM_SDK_TARGET_ID=${GAME_PLATFORM_TARGET_ID}
    PLATFORM_SDK_CURRENT_ID=${GAME_PLATFORM_SDK_ID}
    PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED=${GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED}
    PLATFORM_SDK_ADS_SUPPORTED=${GAME_PLATFORM_ADS_SUPPORTED}
    PLATFORM_SDK_REWARDED_SUPPORTED=${GAME_PLATFORM_REWARDED_SUPPORTED}
    PLATFORM_SDK_STORAGE_SUPPORTED=${GAME_PLATFORM_STORAGE_SUPPORTED}
    GAME_PLATFORM_SDK_DEBUG_UI=$<BOOL:${GAME_PLATFORM_SDK_DEBUG_UI}>
    NT_INTROSPECT_ENABLED=$<BOOL:${NT_INTROSPECT_ENABLED}>
    NT_INTROSPECT_WRITE_ENABLED=$<BOOL:${NT_INTROSPECT_WRITE_ENABLED}>
    _CRT_SECURE_NO_WARNINGS
)
# E4: FEATURE_GAME_ANALYTICS is defined ALWAYS (1/0) at target level -- game_features.c and
# main.c carry #if FEATURE_GAME_ANALYTICS unconditionally, and an undefined macro under
# -Wundef + -Werror would fail native-debug/release. Mirrors NT_DEVAPI_ENABLED=0.
target_compile_definitions(${GAME_TARGET} PRIVATE
    FEATURE_GAME_ANALYTICS=$<BOOL:${GAME_ANALYTICS_ENABLED}>
    FEATURE_GAME_EVENTS=1
    GAME_EVENTS_LOG_MIRROR=$<BOOL:${GAME_EVENTS_LOG_MIRROR}>)
# GAME_STORAGE_APP_ID namespaces the shared web-origin (itch) localStorage; "template" is
# the template's own default, games override it (A2).
target_compile_definitions(${GAME_TARGET} PRIVATE GAME_STORAGE_APP_ID="web-dressup")
if(NOT GAME_DEVAPI_ENABLED)
    target_compile_definitions(${GAME_TARGET} PRIVATE NT_DEVAPI_ENABLED=0)
endif()
nt_set_warning_flags(${GAME_TARGET})
# Match the sanitizer posture of the engine archives we link: the engine
# instruments Debug modules (non-Windows/Clang targets, incl. wasm) and expects
# the consumer executable to carry the same flags -- without this the Debug
# wasm link dies on undefined __asan_*/__ubsan_* (lead decision 2026-07-07).
nt_set_sanitizer_flags(${GAME_TARGET})
set_target_properties(${GAME_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}")

if(GAME_DEVAPI_ENABLED AND NOT EMSCRIPTEN)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_custom_target(devapi_smoke
            COMMAND ${CMAKE_COMMAND} -E env
                "AI_STUDIO_GAME_EXE=$<TARGET_FILE:${GAME_TARGET}>"
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/devapi/smoke_bot.py"
                "--exe" "$<TARGET_FILE:${GAME_TARGET}>"
            DEPENDS ${GAME_TARGET}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
            COMMENT "Running template DevAPI smoke bot"
            VERBATIM
        )
        add_custom_target(quality_responsive
            COMMAND ${CMAKE_COMMAND} -E env
                "AI_STUDIO_GAME_EXE=$<TARGET_FILE:${GAME_TARGET}>"
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/devapi/responsive_viewports.py"
                "--exe" "$<TARGET_FILE:${GAME_TARGET}>"
            DEPENDS ${GAME_TARGET}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
            COMMENT "Capturing QCLR_002 responsive viewport evidence"
            VERBATIM
        )
    endif()
endif()

# --- native C unit tests (Unity + CTest); introduced in A1, extended in A2/A3 ---
if(NOT EMSCRIPTEN)
    enable_testing()
    add_executable(test_game_state_json tests/test_game_state_json.c src/game_state_json.c)
    target_link_libraries(test_game_state_json PRIVATE cjson unity)
    target_include_directories(test_game_state_json PRIVATE src)
    target_compile_definitions(test_game_state_json PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_state_json PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_state_json COMMAND test_game_state_json)

    add_executable(test_game_storage tests/test_game_storage.c src/game_storage.c)
    # nt_log: game_storage.c warns via nt_log_warn on a read ERROR (nt_hash/nt_core are nt_log's deps).
    target_link_libraries(test_game_storage PRIVATE unity nt_log nt_core nt_hash)
    target_include_directories(test_game_storage PRIVATE src)
    target_compile_definitions(test_game_storage PRIVATE
        GAME_STORAGE_APP_ID="template_test" _CRT_SECURE_NO_WARNINGS)
    # MoveFileExA (native quarantine/atomic-replace) is in kernel32, linked by default.
    set_target_properties(test_game_storage PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_storage COMMAND test_game_storage
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    add_executable(test_game_save
        tests/test_game_save.c
        src/game_save.c src/game_storage.c src/game_state_json.c)
    # nt_log: game_save.c/game_storage.c warn via nt_log_warn on the read-error path.
    target_link_libraries(test_game_save PRIVATE cjson unity nt_log nt_core nt_hash)
    target_include_directories(test_game_save PRIVATE src)
    target_compile_definitions(test_game_save PRIVATE
        GAME_SAVE_TESTING=1
        GAME_STORAGE_APP_ID="template_test"
        GAME_SAVE_AUTOSAVE_SLOT="test_slot"
        GAME_SAVE_DEBOUNCE_MS=2000
        GAME_SAVE_MAX_INTERVAL_MS=30000
        GAME_SAVE_DOC_VERSION=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_save PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_save COMMAND test_game_save
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    # (1) основной: позитив + death-тесты переполнения/фазы (NT_ASSERT ФИРИТ)
    add_executable(test_game_events tests/test_game_events.c "${GAME_EVENTS_SRC}/game_events.c"
        "${ENGINE_DIR}/tests/unit/test_helpers/nt_assert_trap.c")
    target_link_libraries(test_game_events PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events PRIVATE "${GAME_EVENTS_INC}" src "${ENGINE_DIR}/tests/unit")
    target_compile_definitions(test_game_events PRIVATE
        GAME_EVENTS_ARENA_BYTES=1024u   # маленькая арена -> переполнение дёшево (позитив влезает)
        GAME_EVENTS_LOG_CAP=64          # маленький кап лога -> переполнение дёшево
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events COMMAND test_game_events)

    # (2) overflow-drop: тот же файл + GAME_EVENTS_SOFT_OVERFLOW=1 -> emit ДРОПАЕТ
    # (не assert'ит) в debug-ctest, проверяет release-семантику (тест #10). Не
    # звено с nt_assert_trap: этот бинарь не использует NT_TEST_EXPECT_ASSERT
    # (test_game_events.c гейтит его #include под #ifndef GAME_EVENTS_SOFT_OVERFLOW).
    add_executable(test_game_events_overflow tests/test_game_events.c "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_events_overflow PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events_overflow PRIVATE "${GAME_EVENTS_INC}" src)
    target_compile_definitions(test_game_events_overflow PRIVATE
        GAME_EVENTS_SOFT_OVERFLOW=1     # выключить debug-assert переполнения -> тестировать дроп
        GAME_EVENTS_ARENA_BYTES=1024u GAME_EVENTS_LOG_CAP=64
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_overflow PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_overflow COMMAND test_game_events_overflow)

    # A4: round-trip gate for the generated fragment state layer. Links the
    # generated game_state.c (data + static wrappers, no game_save_* calls) so
    # game_save.c is NOT needed and the engine is not pulled in. The generated
    # source is an add_custom_command OUTPUT, so regen is an automatic build
    # prerequisite. GAME_STATE_GENERATED_* is always defined (state is always on).
    add_executable(test_game_state_roundtrip
        tests/test_game_state_roundtrip.c
        "${GAME_STATE_GENERATED_SOURCE}" src/game_state_json.c)
    target_link_libraries(test_game_state_roundtrip PRIVATE cjson unity)
    target_include_directories(test_game_state_roundtrip PRIVATE src "${GAME_STATE_GENERATED_DIR}")
    target_compile_definitions(test_game_state_roundtrip PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_state_roundtrip PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_state_roundtrip COMMAND test_game_state_roundtrip)

    # E2: typed event layer round-trip over the COMMITTED golden mini events + frozen
    # E1 transport (no build-time generation). Unconditional -- the golden is a
    # committed fixture and game_events.{c,h}/game_event_desc.h are always compiled.
    # This double-serves as a compile check of the golden.
    add_executable(test_game_events_typed
        tests/test_game_events_typed.c
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_events_typed PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events_typed PRIVATE
        "${GAME_EVENTS_INC}"
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_events_typed PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_typed PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_typed COMMAND test_game_events_typed)

    # E3: descriptor-driven renderer over the COMMITTED golden mini events + frozen E1
    # transport. Native, no devapi (renderer is pure). Unconditional (golden is committed).
    # nt_hash inherits the preset's NT_HASH_LABELS -> asserts are label-agnostic (HIGH-1):
    # devapi-debug exercises the label branch, native-debug the hex branch.
    add_executable(test_game_event_render
        tests/test_game_event_render.c
        "${GAME_EVENTS_SRC}/game_event_render.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_event_render PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_event_render PRIVATE
        "${GAME_EVENTS_INC}" src "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_event_render PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_event_render PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_event_render COMMAND test_game_event_render)

    # E4: analytics writer over the COMMITTED golden mini events + the built-in log type +
    # frozen E1 transport. Native, sink + clock injected (GAME_ANALYTICS_TESTING). Links
    # game_log.c (case #7 emits a log event). Label-agnostic asserts.
    add_executable(test_game_analytics
        tests/test_game_analytics.c
        "${GAME_EVENTS_SRC}/game_analytics.c"
        "${GAME_EVENTS_SRC}/game_event_render.c" # E3 renderer (reused)
        src/game_log.c            # case #7 game_log_emit
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_analytics PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_analytics PRIVATE
        "${GAME_EVENTS_INC}" src "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_analytics PRIVATE
        FEATURE_GAME_ANALYTICS=1 GAME_ANALYTICS_TESTING=1
        GAME_ANALYTICS_BUF_BYTES=256u        # small buffer -> threshold/drop are cheap
        GAME_ANALYTICS_FLUSH_BYTES=192u
        GAME_STORAGE_APP_ID="template_test"  # header app field
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_analytics PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_analytics COMMAND test_game_analytics)

    add_executable(test_game_events_log_mirror
        tests/test_game_events_log_mirror.c
        "${GAME_EVENTS_SRC}/game_events_log_mirror.c"
        "${GAME_EVENTS_SRC}/game_event_render.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_events_log_mirror PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_events_log_mirror PRIVATE
        "${GAME_EVENTS_INC}" src "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_events_log_mirror PRIVATE
        GAME_EVENTS_LOG_MIRROR=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_log_mirror PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_log_mirror COMMAND test_game_events_log_mirror)

    # E2 (M3): warning-gated compile check of the richest generated branches
    # (i64/f64/hash/bool/bytes+len, union staging, offset arithmetic) live ONLY in
    # mini. An OBJECT lib compiles the golden mini events source under the SAME
    # -W set + -Werror as the game target, WITHOUT Unity (compile-only, no link) so
    # -Wconversion is not fighting the test framework's macros.
    add_library(check_mini_state_events OBJECT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c")
    target_include_directories(check_mini_state_events PRIVATE
        "${GAME_EVENTS_INC}"
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_link_libraries(check_mini_state_events PRIVATE nt_hash nt_log nt_core)  # headers only (OBJECT does not link)
    nt_set_warning_flags(check_mini_state_events)  # same -W set + -Werror toggle as the game target

    # И2a: catalog-lookup over the generated const tables (content codegen, §6.3/§6.8).
    add_executable(test_items_catalog
        tests/test_items_catalog.c
        "${ITEMS_CORE_SRC}/items_catalog.c"
        "${ITEMS_CATALOG_GENERATED_SOURCE}")
    target_link_libraries(test_items_catalog PRIVATE unity)
    target_include_directories(test_items_catalog PRIVATE "${ITEMS_CORE_INC}" src "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_items_catalog PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_items_catalog PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_catalog COMMAND test_items_catalog)

    # И2a skeleton -> И2b full (§6.6c/§6.8): round-trip over the generated items
    # fragment state layer. Links items_state.c + the (non-empty, R2) events source +
    # the real on_new_game/reconcile bodies (Р9 §7.3) + gsj_* toolkit + game_events.c
    # (event descriptor plumbing) + И2b ownership (items_containers.c) + the catalog
    # (item_core, reconcile/containers need it, code-review #11). game_save.c is
    # NOT linked (engine-independent) -- the test TU stubs game_save_mark_dirty()
    # itself instead (precedent: test_game_state_roundtrip, code-review #11).
    add_executable(test_items_fragment
        tests/test_items_fragment.c
        "${ITEMS_STATE_GENERATED_SOURCE}"
        "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        src/features/items/items_bootstrap.c
        "${ITEMS_CORE_SRC}/items_reconcile.c"
        "${ITEMS_CORE_SRC}/items_containers.c"
        "${ITEMS_CORE_SRC}/items_catalog.c"
        "${ITEMS_CATALOG_GENERATED_SOURCE}"
        src/game_state_json.c
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_items_fragment PRIVATE cjson unity nt_hash nt_log nt_core)
    target_include_directories(test_items_fragment PRIVATE "${ITEMS_CORE_INC}" "${GAME_EVENTS_INC}" src "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_items_fragment PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_items_fragment PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_fragment COMMAND test_items_fragment
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

    # F1 (deep-review, T0327 И2c destructive-change guard follow-up): the
    # guard was VOLUNTARY until this line -- items_ops.py validate ran only
    # when a human remembered to invoke it by hand. Wiring it into ctest
    # means deleting a shipped def_id without a lock.removed reaction now
    # turns the BUILD's test suite red automatically; the forcing chain
    # (§ README "Lock workflow") finally starts at an automated gate, not a
    # manual step. Reads only committed source (content/items.json,
    # content/items.lock.json, state/items.schema.json) -- no generated-file
    # dependency, so no add_dependencies needed. WORKING_DIRECTORY is the
    # template root so the CWD-relative --catalog/--schema/--baseline/
    # --state-schema/--src-dir arguments resolve the same way a human running
    # the command from templates/template/ would see it.
    # T0337 M1 (H2/R7, CRITICAL): items_ops.py's own argparse defaults are
    # SCRIPT-relative (Path(__file__).parent.parent). After the move to
    # features/items-core/scripts/, those defaults would resolve into a
    # nonexistent features/items-core/content/ -- pass every path explicitly,
    # CWD-relative to the template root (WORKING_DIRECTORY below). Principle:
    # the module CLI takes paths ONLY from the caller (CWD/args), never from
    # __file__. --src-dir now points at the game-side items corner only
    # (src/features/items/) -- the display_name-keying lint no longer scans
    # the (relocated) ownership core; see items README L5-note.
    add_test(NAME items_ops_validate COMMAND "${Python3_EXECUTABLE}"
        "${ITEMS_CORE_SCRIPTS}/items_ops.py" validate
        --catalog content/items.json --schema content/item_fields.schema.json
        --baseline content/items.lock.json --state-schema state/items.schema.json
        --src-dir src/features/items
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    # F4 (deep-review): committed proof that the lock-workflow rules
    # themselves actually fire correctly (removed-without-reaction,
    # removed-version-not-shipped, lock-inconsistent, the F3 malformed-lock
    # IO error, the happy batch path) -- not just a manually re-run scratch
    # check. unittest against temp fixtures, precedent
    # features/game-state/scripts/generate_state_test.py.
    add_test(NAME items_ops_test COMMAND "${Python3_EXECUTABLE}" "${ITEMS_CORE_SCRIPTS}/items_ops_test.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    # И3a (§5.9d, H2/R11): test_progression compiles progression.c, which
    # #includes progression_tracks.gen.h -- but does NOT link
    # progression_tracks.gen.c (it links its OWN hand-written k_tracks catalog,
    # tests/test_progression_catalog.c, to avoid a duplicate-symbol link error,
    # R10). Without a linked OUTPUT of the codegen custom_command, ninja has no
    # dependency edge forcing the .gen.h to exist before progression.c compiles
    # on a clean/parallel build -- a phony target + add_dependencies closes that
    # gap (items avoids this because it LINKS its generated .gen.c, §5.9d).
    add_custom_target(progression_tracks_gen DEPENDS
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.h"
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c")

    add_executable(test_progression
        tests/test_progression.c
        tests/test_progression_catalog.c                        # РУКОПИСНЫЙ k_tracks (НЕ .gen.c, R10)
        "${PROGRESSION_CORE_SRC}/progression.c"                   # T0337 M2: in-place module
        "${PROGRESSION_STATE_GENERATED_SOURCE}"                  # progression_state.c
        "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}"           # progression_state_events.gen.c
        "${ITEMS_CORE_SRC}/items_containers.c"                    # items runtime (progression spends/reads purse; T0337 M1: in-place module)
        "${ITEMS_CORE_SRC}/items_catalog.c" "${ITEMS_CATALOG_GENERATED_SOURCE}"
        "${ITEMS_STATE_GENERATED_SOURCE}" "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        src/features/items/items_bootstrap.c                     # items on_new_game (link completeness)
        "${ITEMS_CORE_SRC}/items_reconcile.c"                     # items reconcile (T0337 M1 split, link completeness)
        src/game_state_json.c "${GAME_EVENTS_SRC}/game_events.c")
    add_dependencies(test_progression progression_tracks_gen)   # H2: guarantees progression_tracks.gen.h before progression.c compiles
    target_link_libraries(test_progression PRIVATE cjson unity nt_hash nt_log nt_core)
    target_include_directories(test_progression PRIVATE "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${GAME_EVENTS_INC}" src "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_progression PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_progression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_progression COMMAND test_progression WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

    # GOLDEN (§5.8b): links the DEMO progression_tracks.gen.c (generation
    # auto-triggers via the custom_command OUTPUT, precedent items :593) -- the
    # ONLY target in this file that links the real generated catalog; no
    # duplicate-k_tracks conflict with test_progression (which never links it).
    add_executable(test_progression_curve
        tests/test_progression_curve.c
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c")
    target_link_libraries(test_progression_curve PRIVATE unity)
    # T0337 M1 added ITEMS_CORE_INC here ahead of schedule (spec §5.6 labels it
    # [M2] alongside PROGRESSION_CORE_INC) because items.h had already moved out
    # of src/ in M1 while progression.h had not: progression_tracks.gen.h
    # includes features/progression/progression.h (then still src/) which
    # includes features/items/items.h (H1 chain, §13) -- without ITEMS_CORE_INC
    # this target would have failed to compile mid-M1, breaking the
    # per-increment-green invariant (§9/R8). M2 now moves progression.h itself
    # out of src/ (§3.3), so PROGRESSION_CORE_INC completes the include path.
    target_include_directories(test_progression_curve PRIVATE "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" src "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_progression_curve PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_progression_curve PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_progression_curve COMMAND test_progression_curve)

    # И3b (§6.1/G8): L0 int64-abbrev formatter -- pure, no generated-file/state
    # dependency, so a plain two-file test target (precedent test_game_state_json).
    add_executable(test_game_format tests/test_game_format.c src/game_format.c)
    target_link_libraries(test_game_format PRIVATE unity)
    target_include_directories(test_game_format PRIVATE src)
    target_compile_definitions(test_game_format PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_format PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_format COMMAND test_game_format)

    add_executable(test_dress_room
        tests/test_dress_room.c
        src/features/dress_room/dress_room_state.c)
    target_link_libraries(test_dress_room PRIVATE unity)
    target_include_directories(test_dress_room PRIVATE src)
    target_compile_definitions(test_dress_room PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_dress_room PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_dress_room COMMAND test_dress_room)

    add_executable(test_platform_sdk
        tests/test_platform_sdk.c
        "${PLATFORM_SDK_SRC}/platform_sdk.c")
    target_link_libraries(test_platform_sdk PRIVATE unity)
    target_include_directories(test_platform_sdk PRIVATE "${PLATFORM_SDK_INC}")
    target_compile_definitions(test_platform_sdk PRIVATE
        PLATFORM_SDK_TARGET_ID=${GAME_PLATFORM_TARGET_ID}
        PLATFORM_SDK_CURRENT_ID=${GAME_PLATFORM_SDK_ID}
        PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED=${GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED}
        PLATFORM_SDK_ADS_SUPPORTED=${GAME_PLATFORM_ADS_SUPPORTED}
        PLATFORM_SDK_REWARDED_SUPPORTED=${GAME_PLATFORM_REWARDED_SUPPORTED}
        PLATFORM_SDK_STORAGE_SUPPORTED=${GAME_PLATFORM_STORAGE_SUPPORTED}
        PLATFORM_SDK_TESTING=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_platform_sdk PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_platform_sdk COMMAND test_platform_sdk)

    add_executable(test_platform_lifecycle
        tests/test_platform_lifecycle.c
        src/platform_lifecycle.c
        "${PLATFORM_SDK_SRC}/platform_sdk.c")
    target_link_libraries(test_platform_lifecycle PRIVATE unity nt_input_stub)
    target_include_directories(test_platform_lifecycle PRIVATE src "${PLATFORM_SDK_INC}")
    target_compile_definitions(test_platform_lifecycle PRIVATE
        PLATFORM_SDK_TARGET_ID=${GAME_PLATFORM_TARGET_ID}
        PLATFORM_SDK_CURRENT_ID=${GAME_PLATFORM_SDK_ID}
        PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED=${GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED}
        PLATFORM_SDK_ADS_SUPPORTED=${GAME_PLATFORM_ADS_SUPPORTED}
        PLATFORM_SDK_REWARDED_SUPPORTED=${GAME_PLATFORM_REWARDED_SUPPORTED}
        PLATFORM_SDK_STORAGE_SUPPORTED=${GAME_PLATFORM_STORAGE_SUPPORTED}
        PLATFORM_SDK_TESTING=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_platform_lifecycle PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_platform_lifecycle COMMAND test_platform_lifecycle)

    add_executable(test_platform_sdk_events
        tests/test_platform_sdk_events.c
        "${PLATFORM_SDK_SRC}/platform_sdk.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_platform_sdk_events PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_platform_sdk_events PRIVATE "${PLATFORM_SDK_INC}" "${GAME_EVENTS_INC}")
    target_compile_definitions(test_platform_sdk_events PRIVATE
        PLATFORM_SDK_TARGET_ID=${GAME_PLATFORM_TARGET_ID}
        PLATFORM_SDK_CURRENT_ID=${GAME_PLATFORM_SDK_ID}
        PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED=${GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED}
        PLATFORM_SDK_ADS_SUPPORTED=${GAME_PLATFORM_ADS_SUPPORTED}
        PLATFORM_SDK_REWARDED_SUPPORTED=${GAME_PLATFORM_REWARDED_SUPPORTED}
        PLATFORM_SDK_STORAGE_SUPPORTED=${GAME_PLATFORM_STORAGE_SUPPORTED}
        PLATFORM_SDK_TESTING=1
        FEATURE_GAME_EVENTS=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_platform_sdk_events PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_platform_sdk_events COMMAND test_platform_sdk_events)

    find_program(Node_EXECUTABLE node)
    if(Node_EXECUTABLE)
        add_test(NAME platform_sdk_node_test
            COMMAND "${Node_EXECUTABLE}" --test features/platform-sdk/tests/platform_sdk.test.mjs
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    endif()

    # T0327 tail: 4-fragment composition test -- lifts settings/items/progression/game
    # through the REAL game_save registry (envelope + on_new_game fan-out + skip-reset).
    # GAME_SAVE_TESTING injects clocks & avoids nt_time (precedent test_game_save).
    add_executable(test_template_composition
        tests/test_template_composition.c
        src/game_save.c src/game_storage.c src/game_state_json.c "${GAME_EVENTS_SRC}/game_events.c"
        "${GAME_STATE_GENERATED_SOURCE}" "${GAME_STATE_GENERATED_EVENTS_SOURCE}"
        "${SETTINGS_STATE_GENERATED_SOURCE}" src/features/settings/settings.c
        "${ITEMS_STATE_GENERATED_SOURCE}" "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        src/features/items/items_bootstrap.c
        "${ITEMS_CORE_SRC}/items_reconcile.c" "${ITEMS_CORE_SRC}/items_containers.c"
        "${ITEMS_CORE_SRC}/items_catalog.c" "${ITEMS_CATALOG_GENERATED_SOURCE}"
        "${PROGRESSION_STATE_GENERATED_SOURCE}" "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}"
        "${PROGRESSION_CORE_SRC}/progression.c"
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c")   # REAL hero curve (cf. test_progression_curve)
    add_dependencies(test_template_composition progression_tracks_gen)  # progression.c #includes .gen.h
    # nt_ui_interface (review #1 smoke-check): settings.h pulls ui/nt_ui.h for
    # nt_ui_context_t; header-only include-root + NT_UI_DEBUG_TOOLS define, no
    # Clay/impl chain -- draw_ui is never called in this TU, only declared.
    target_link_libraries(test_template_composition PRIVATE cjson unity nt_hash nt_log nt_core nt_ui_interface)
    target_include_directories(test_template_composition PRIVATE
        "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${GAME_EVENTS_INC}" src
        "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_template_composition PRIVATE
        GAME_SAVE_TESTING=1 GAME_STORAGE_APP_ID="template_test"
        GAME_SAVE_AUTOSAVE_SLOT="test_composition"
        GAME_SAVE_DEBOUNCE_MS=2000 GAME_SAVE_MAX_INTERVAL_MS=30000 GAME_SAVE_DOC_VERSION=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_template_composition PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_template_composition COMMAND test_template_composition
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

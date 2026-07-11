cmake_minimum_required(VERSION 3.25)
project(game LANGUAGES C)

include(cmake/GameOptions.cmake)

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

include(cmake/GameAssets.cmake)

# --- game executable (the conductor + systems) ---
add_executable(${GAME_TARGET}
    src/main.c
    src/platform_lifecycle.c
    "${GAME_EVENTS_SRC}/game_events.c" # E1
    src/game_log.c               # E4.B: built-in "log" event type (unconditional leaf)
    src/features/game_features.c # E1
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
    src/ui/demo_hud.c                                # И3b: game composition (resource_panel over items/progression)
)
if(NOT EMSCRIPTEN)
    add_dependencies(${GAME_TARGET} game_asset_packs)
endif()
# Game-state provisioning is always on (lead decision 2026-07-07: "a game without
# state is impossible"); the former on/off CMake axis is gone (И2-0).

include(cmake/GameCodegen.cmake)

include(cmake/GamePlatform.cmake)

include(cmake/GameTests.cmake)

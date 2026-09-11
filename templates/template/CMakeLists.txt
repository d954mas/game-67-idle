cmake_minimum_required(VERSION 3.25)
project(game LANGUAGES C)

include(cmake/GameOptions.cmake)

# Shared engine (one copy, referenced not copied).
set(ENGINE_DIR "${GAME_REPO_ROOT}/external/neotolis-engine")
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

# T0337: in-place modules (precedent features/game-state).
set(ITEMS_CORE_DIR        "${GAME_REPO_ROOT}/features/items-core")
set(ITEMS_CORE_INC        "${ITEMS_CORE_DIR}/include")
set(ITEMS_CORE_SRC        "${ITEMS_CORE_DIR}/src")
set(ITEMS_CORE_SCRIPTS    "${ITEMS_CORE_DIR}/scripts")
# T0337 M2: progression-core (L2 -> items-core L1, §2.4). Same depth-2 invariant as above.
set(PROGRESSION_CORE_DIR     "${GAME_REPO_ROOT}/features/progression-core")
set(PROGRESSION_CORE_INC     "${PROGRESSION_CORE_DIR}/include")
set(PROGRESSION_CORE_SRC     "${PROGRESSION_CORE_DIR}/src")
set(PROGRESSION_CORE_SCRIPTS "${PROGRESSION_CORE_DIR}/scripts")
set(PLATFORM_SDK_DIR         "${GAME_REPO_ROOT}/features/platform-sdk")
set(PLATFORM_SDK_INC         "${PLATFORM_SDK_DIR}/include")
set(PLATFORM_SDK_SRC         "${PLATFORM_SDK_DIR}/src")
set(PLATFORM_SDK_WEB         "${PLATFORM_SDK_DIR}/web")
set(GAME_EVENTS_DIR          "${GAME_REPO_ROOT}/features/game-events")
set(GAME_EVENTS_INC          "${GAME_EVENTS_DIR}/include")
set(GAME_EVENTS_SRC          "${GAME_EVENTS_DIR}/src")
set(SCENES_CORE_DIR          "${GAME_REPO_ROOT}/features/scenes-core")
set(SCENES_CORE_INC          "${SCENES_CORE_DIR}/include")
set(SCENES_CORE_SRC          "${SCENES_CORE_DIR}/src")
set(GAME_STATE_DIR           "${GAME_REPO_ROOT}/features/game-state")
set(GAME_STATE_INC           "${GAME_STATE_DIR}/include")
set(GAME_STATE_SRC           "${GAME_STATE_DIR}/src")
set(UI_KIT_DIR               "${GAME_REPO_ROOT}/features/ui-kit")
set(UI_KIT_INC               "${UI_KIT_DIR}/include")
set(UI_KIT_SRC               "${UI_KIT_DIR}/src")
set(UI_KIT_TOKENS            "${UI_KIT_DIR}/tokens")
set(UI_KIT_TOOLS             "${UI_KIT_DIR}/tools")
set(LOCALIZATION_DIR         "${GAME_REPO_ROOT}/features/localization")
set(LOCALIZATION_INC         "${LOCALIZATION_DIR}/include")
set(LOCALIZATION_SRC         "${LOCALIZATION_DIR}/src")
set(LOCALIZATION_SCRIPTS     "${LOCALIZATION_DIR}/scripts")
set(LEADERBOARD_DIR          "${GAME_REPO_ROOT}/features/leaderboard")
set(LEADERBOARD_INC          "${LEADERBOARD_DIR}/include")
set(LEADERBOARD_SRC          "${LEADERBOARD_DIR}/src")
# Every backend compiles in; the board manifest picks one at init.
set(LEADERBOARD_SOURCES
    "${LEADERBOARD_SRC}/leaderboard.c"
    "${LEADERBOARD_SRC}/leaderboard_core.c"
    "${LEADERBOARD_SRC}/leaderboard_extra.c"
    "${LEADERBOARD_SRC}/leaderboard_mock.c"
    "${LEADERBOARD_SRC}/leaderboard_http.c"
    "${LEADERBOARD_SRC}/leaderboard_portal.c")
set(REMOTE_IMAGE_DIR         "${GAME_REPO_ROOT}/features/remote-image")
set(REMOTE_IMAGE_INC         "${REMOTE_IMAGE_DIR}/include")
set(REMOTE_IMAGE_SRC         "${REMOTE_IMAGE_DIR}/src")
set(REMOTE_IMAGE_SOURCES
    "${REMOTE_IMAGE_SRC}/remote_image.c"
    "${REMOTE_IMAGE_SRC}/remote_image_engine.c")

set(GAME_TARGET game)
set(GAME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(GAME_PACK_DIR "${CMAKE_BINARY_DIR}/pack")
set(GAME_ASSETS_DIR "${GAME_OUTPUT_DIR}/assets")
set(GAME_SOURCE_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")

# --- game executable (the conductor + systems) ---
add_executable(${GAME_TARGET}
    src/main.c
    src/game_asset_paths.c
    src/game_input.c
    "${SCENES_CORE_SRC}/scene_manager.c"
    src/game_scenes.c
    src/platform_lifecycle.c
    "${GAME_EVENTS_SRC}/game_events.c" # E1
    src/game_log.c               # E4.B: built-in "log" event type (unconditional leaf)
    src/features/game_features.c # E1
    src/ui/hud.c
    src/ui/focus_prompt_ui.c
    src/ui/platform_sdk_debug.c
    src/ui/ui_runtime.c
    src/ui/theme.c                                   # binds THIS game's atlas to the ui-kit feature
    "${UI_KIT_SRC}/ui_tokens.c"
    "${UI_KIT_SRC}/ui_theme.c"
    "${UI_KIT_SRC}/ui_metrics.c"
    "${UI_KIT_SRC}/ui_kit.c"
    "${UI_KIT_SRC}/ui_safe_area.c"
    "${UI_KIT_SRC}/ui_reach.c"
    src/render/render_mesh.c
    src/render/shader_programs.c
    src/render/capture.c
    src/systems/sys_move.c
    src/systems/sys_platform_hooks.c                 # attention/ad freeze, mixer mute, ad points as API
    "${GAME_STATE_SRC}/game_save_sync.c"
    "${GAME_STATE_SRC}/game_save_cloud.c"
    src/game_save_policy.c
    src/game_save_policy_validator.c
    src/systems/sys_cloud_save.c                     # portal transport binding for shared save synchronization
    src/systems/sys_portal_metrics.c                 # frame events -> the portal's funnel, through a game mapper
    src/features/settings/settings_screen.c
    "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c" # И3a: const progression track/curve tables (content codegen)
    src/game_format.c                                # И3b: L0 int64-abbrev formatter
    "${PLATFORM_SDK_SRC}/platform_sdk.c"             # T0339: target/sdk identity + policy capabilities
    "${PLATFORM_SDK_SRC}/platform_sdk_storage.c"
    "${PLATFORM_SDK_SRC}/platform_sdk_cloud.c"       # portal player storage: the account's copy of the save
    src/features/resource_panel/resource_panel.c     # И3b: generic counter/bar widget (zero feature headers)
    src/ui/demo_hud.c                                # И3b: game composition (resource_panel over items/progression)
    src/ui/loc_widgets.c                             # the ONLY caller of the raw text entry points
    ${LEADERBOARD_SOURCES}                           # capability facade + backends; inert until a game calls leaderboard_init
    ${REMOTE_IMAGE_SOURCES}                          # url -> texture cache; inert until a game calls remote_image_init
    # scene-scaffold:sources
)
target_include_directories(${GAME_TARGET} PRIVATE "${SCENES_CORE_INC}" "${UI_KIT_INC}" "${LEADERBOARD_INC}" "${REMOTE_IMAGE_INC}")
include("${REMOTE_IMAGE_DIR}/cmake/RemoteImage.cmake")
remote_image_ensure_stb("${ENGINE_DIR}")
target_link_libraries(${GAME_TARGET} PRIVATE nt_base64 stb_image)
target_compile_definitions(${GAME_TARGET} PRIVATE FEATURE_SCENES_CORE=1 FEATURE_UI_KIT=1)
if(GAME_DEBUG_SCENES_ENABLED)
    target_compile_definitions(${GAME_TARGET} PRIVATE
        GAME_DEBUG_SCENES_ENABLED=1)
else()
    target_compile_definitions(${GAME_TARGET} PRIVATE
        GAME_DEBUG_SCENES_ENABLED=0)
endif()
if(GAME_DEVAPI_ENABLED)
    target_sources(${GAME_TARGET} PRIVATE
        "${SCENES_CORE_SRC}/scene_manager_devapi.c")
endif()
# scene-scaffold:debug-sources
# Game-state provisioning is always on (lead decision 2026-07-07: "a game without
# state is impossible"); the former on/off CMake axis is gone (И2-0).

include(cmake/GameCodegen.cmake)
include(cmake/GameAssets.cmake)
if(NOT EMSCRIPTEN)
    add_dependencies(${GAME_TARGET} game_asset_packs)
endif()

include(cmake/GamePlatform.cmake)

include(cmake/GameTests.cmake)

if(NOT EMSCRIPTEN)
    # Pure half of the leaderboard pack (day math, wire encoding, parsing,
    # place recalc) and the facade over the mock backend.
    include("${LEADERBOARD_DIR}/cmake/LeaderboardExample.cmake")
    leaderboard_add_example_test()
    game_add_c_test(test_leaderboard_core WARNINGS
        SOURCES "${LEADERBOARD_DIR}/tests/test_leaderboard_core.c"
                "${LEADERBOARD_SRC}/leaderboard_core.c"
                "${LEADERBOARD_SRC}/leaderboard_extra.c"
        LIBS cjson nt_base64
        INCLUDES "${LEADERBOARD_INC}")
    # The portal backend compiles into every pack target, so each of them needs
    # the platform facade it calls.
    game_add_c_test(test_leaderboard WARNINGS
        SOURCES "${LEADERBOARD_DIR}/tests/test_leaderboard.c" ${LEADERBOARD_SOURCES}
                "${PLATFORM_SDK_SRC}/platform_sdk.c"
        LIBS cjson nt_base64
        DEFINES LEADERBOARD_TESTING=1 PLATFORM_SDK_TESTING=1
        INCLUDES "${LEADERBOARD_INC}" "${PLATFORM_SDK_INC}")
    # The http backend under a canned transport: no engine http, no network,
    # and no other backend.
    game_add_c_test(test_leaderboard_http WARNINGS
        SOURCES "${LEADERBOARD_DIR}/tests/test_leaderboard_http.c"
                "${LEADERBOARD_SRC}/leaderboard.c"
                "${LEADERBOARD_SRC}/leaderboard_core.c"
                "${LEADERBOARD_SRC}/leaderboard_extra.c"
                "${LEADERBOARD_SRC}/leaderboard_http.c"
        LIBS cjson nt_base64
        DEFINES LEADERBOARD_TESTING=1
        INCLUDES "${LEADERBOARD_INC}")
    # The portal backend speaks to the platform facade, so its test links both.
    game_add_c_test(test_leaderboard_portal WARNINGS
        SOURCES "${LEADERBOARD_DIR}/tests/test_leaderboard_portal.c" ${LEADERBOARD_SOURCES}
                "${PLATFORM_SDK_SRC}/platform_sdk.c"
        LIBS cjson nt_base64
        DEFINES LEADERBOARD_TESTING=1 PLATFORM_SDK_TESTING=1
        INCLUDES "${LEADERBOARD_INC}" "${PLATFORM_SDK_INC}")
    # The remote-image table against a canned transport, decoder, GPU and
    # clock; the engine binding stays out of the link. The engine include
    # roots are for the nt_texture_t handle type only.
    game_add_c_test(test_remote_image WARNINGS
        SOURCES "${REMOTE_IMAGE_DIR}/tests/test_remote_image.c"
                "${REMOTE_IMAGE_SRC}/remote_image.c"
        DEFINES REMOTE_IMAGE_TESTING=1
        INCLUDES "${REMOTE_IMAGE_INC}" "${ENGINE_DIR}/engine" "${ENGINE_DIR}/shared/include")
endif()

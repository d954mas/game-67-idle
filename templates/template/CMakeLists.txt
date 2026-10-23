cmake_minimum_required(VERSION 3.25)
project(game LANGUAGES C)

include(cmake/GameOptions.cmake)

set(ENGINE_DIR "${GAME_REPO_ROOT}/external/neotolis-engine")
set(GAME_STATE_DIR "${GAME_REPO_ROOT}/features/game-state")
set(GAME_STATE_INC "${GAME_STATE_DIR}/include")
set(GAME_STATE_SRC "${GAME_STATE_DIR}/src")
set(PLATFORM_SDK_WEB "${GAME_REPO_ROOT}/features/platform-sdk/web")
set(GAME_TARGET game)
set(GAME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(GAME_PACK_DIR "${CMAKE_BINARY_DIR}/pack")
set(GAME_ASSETS_DIR "${GAME_OUTPUT_DIR}/assets")

add_subdirectory("${ENGINE_DIR}" "${CMAKE_BINARY_DIR}/_engine")
if(NOT EMSCRIPTEN AND NOT TARGET nt_builder)
    add_subdirectory("${ENGINE_DIR}/tools/builder" "${CMAKE_BINARY_DIR}/_builder")
endif()

add_executable(${GAME_TARGET}
    src/main.c
    src/shell/shell_platform.c
    src/shell/shell_boot.c
    src/shell/shell_tick.c
    src/shell/shell_present.c
    src/game_input.c
    src/game_music_pack.c
    src/features/game_features.c
    src/render/blank_renderer.c)
target_include_directories(${GAME_TARGET} PRIVATE src "${GAME_STATE_INC}")
target_link_libraries(${GAME_TARGET} PRIVATE
    cjson
    nt_app nt_core nt_gfx nt_basisu_transcoder nt_hash nt_input nt_log
    nt_meshwire nt_time nt_window)
target_compile_definitions(${GAME_TARGET} PRIVATE
    _CRT_SECURE_NO_WARNINGS
    GAME_APP_NAME="${GAME_TITLE}"
    GAME_WINDOW_TITLE="${GAME_WINDOW_TITLE}"
    GAME_SAVE_AUTOSAVE_SLOT="autosave"
    GAME_SAVE_DEBOUNCE_MS=2000
    GAME_SAVE_MAX_INTERVAL_MS=30000
    GAME_SAVE_DOC_VERSION=2)

include(cmake/GameCodegen.cmake)
include(cmake/GameAssets.cmake)
include(cmake/GameTests.cmake)

if(GAME_DEVAPI_ENABLED)
    target_sources(${GAME_TARGET} PRIVATE "${GAME_STATE_SRC}/game_save_devapi.c")
    if(EMSCRIPTEN)
        target_link_libraries(${GAME_TARGET} PRIVATE nt_devapi_default nt_http nt_platform_web nt_ui)
    else()
        target_link_libraries(${GAME_TARGET} PRIVATE
            nt_devapi_default nt_devapi_net nt_fs nt_http_stub nt_platform nt_ui)
    endif()
endif()

if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_platform_web)
    include("${GAME_REPO_ROOT}/features/platform-sdk/cmake/PlatformSdk.cmake")
    platform_sdk_crash(${GAME_TARGET})
    target_sources(${GAME_TARGET} PRIVATE src/runtime_build_marker.c)
    target_compile_definitions(${GAME_TARGET} PRIVATE
        GAME_RUNTIME_BUILD_FINGERPRINT="${GAME_RUNTIME_BUILD_FINGERPRINT}"
        GAME_RUNTIME_BUILD_TARGET="${GAME_PUBLISH_TARGET}"
        GAME_RUNTIME_BUILD_ADAPTER="${GAME_PLATFORM_SDK}"
        GAME_RUNTIME_BUILD_PROFILE="${GAME_RUNTIME_BUILD_PROFILE}"
        GAME_RUNTIME_BUILD_DEBUG_UI="${GAME_RUNTIME_BUILD_DEBUG_UI}"
        GAME_RUNTIME_BUILD_DEVAPI="${GAME_RUNTIME_BUILD_DEVAPI}"
        GAME_RUNTIME_BUILD_ANALYTICS="${GAME_RUNTIME_BUILD_ANALYTICS}"
        GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR="${GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR}")
    target_link_options(${GAME_TARGET} PRIVATE
        "SHELL:-sEXPORTED_FUNCTIONS=_main,_malloc,_free"
        "SHELL:-sMIN_WEBGL_VERSION=2"
        "SHELL:-sMAX_WEBGL_VERSION=2"
        "SHELL:-sFULL_ES3=1")
    if(GAME_DEVAPI_ENABLED)
        target_link_options(${GAME_TARGET} PRIVATE
            "SHELL:-sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString")
    endif()
    configure_file(web/index.html.in "${GAME_OUTPUT_DIR}/index.html" @ONLY)
    add_custom_target(platform_sdk_web_assets
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/platform-sdk.js" "${GAME_OUTPUT_DIR}/platform-sdk.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/adapters/${GAME_PLATFORM_SDK}.js" "${GAME_OUTPUT_DIR}/platform-sdk-adapter.js"
        DEPENDS "${PLATFORM_SDK_WEB}/platform-sdk.js"
                "${PLATFORM_SDK_WEB}/adapters/${GAME_PLATFORM_SDK}.js")
else()
    target_link_libraries(${GAME_TARGET} PRIVATE glad)
endif()

if(NOT EMSCRIPTEN)
    add_dependencies(${GAME_TARGET} game_asset_packs)
endif()
if(NOT GAME_DEVAPI_ENABLED)
    target_compile_definitions(${GAME_TARGET} PRIVATE NT_DEVAPI_ENABLED=0)
endif()
nt_set_warning_flags(${GAME_TARGET})
nt_set_sanitizer_flags(${GAME_TARGET})
set_target_properties(${GAME_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}")

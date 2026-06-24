cmake_minimum_required(VERSION 3.25)
project(game LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(NT_PRESET_NAME "native-release")
else()
    set(NT_PRESET_NAME "native-debug")
endif()

# Shared engine (one copy, referenced not copied). The game folder sits next to
# external/ in the repo, so the engine is one level up.
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/neotolis-engine")
add_subdirectory("${ENGINE_DIR}" "${CMAKE_BINARY_DIR}/_engine")
if(NOT EMSCRIPTEN AND NOT TARGET nt_builder)
    add_subdirectory("${ENGINE_DIR}/tools/builder" "${CMAKE_BINARY_DIR}/_builder")
endif()

set(GAME_TARGET game)
set(GAME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(GAME_PACK_DIR "${CMAKE_BINARY_DIR}/pack")
set(GAME_ASSETS_DIR "${GAME_OUTPUT_DIR}/assets")

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
    src/ui/hud.c
)
if(NOT EMSCRIPTEN)
    add_dependencies(${GAME_TARGET} game_asset_packs)
endif()
target_include_directories(${GAME_TARGET} PRIVATE src "${ENGINE_DIR}/deps/glfw/deps")
target_link_libraries(${GAME_TARGET} PRIVATE
    nt_core nt_app nt_input nt_window nt_gfx nt_text_renderer nt_font
    nt_resource nt_material nt_shared nt_render nt_hash nt_fs nt_log nt_math nt_time
    glad stb_image_write
)
if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http nt_platform_web)
else()
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http_stub)
endif()
if(WIN32 AND NOT EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE winmm)
endif()
target_compile_definitions(${GAME_TARGET} PRIVATE
    NT_DEVAPI_ENABLED=0
    GAME_ASSET_PACK_PATH="${GAME_ASSETS_DIR}/game.ntpack"
    _CRT_SECURE_NO_WARNINGS
)
nt_set_warning_flags(${GAME_TARGET})
set_target_properties(${GAME_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}")

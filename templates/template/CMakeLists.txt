cmake_minimum_required(VERSION 3.25)
project(game LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(NT_PRESET_NAME "native-release")
else()
    set(NT_PRESET_NAME "native-debug")
endif()

if(NOT DEFINED GAME_DEVAPI_ENABLED)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(GAME_DEVAPI_ENABLED OFF CACHE BOOL "Build the template with engine DevAPI automation support." FORCE)
    else()
        set(GAME_DEVAPI_ENABLED ON CACHE BOOL "Build the template with engine DevAPI automation support." FORCE)
    endif()
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

# Shared engine (one copy, referenced not copied). Templates and games live one
# folder below the repo groups, so the engine is two levels up.
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../external/neotolis-engine")
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
            assets/shaders/mesh_tex.vert assets/shaders/mesh_tex.frag
            assets/shaders/sprite.vert assets/shaders/sprite.frag
            assets/ui/panel.png assets/ui/button.png
            assets/ui/slider_track.png assets/ui/slider_fill.png assets/ui/slider_thumb.png
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
    src/devapi/game_state_devapi.c
    src/ui/hud.c
    src/ui/ui_runtime.c
    src/ui/theme.c
    src/render/render_mesh.c
    src/render/capture.c
    src/systems/sys_move.c
    src/systems/sys_settings.c
)
if(NOT EMSCRIPTEN)
    add_dependencies(${GAME_TARGET} game_asset_packs)
endif()
target_include_directories(${GAME_TARGET} PRIVATE src "${ENGINE_DIR}/deps/glfw/deps")
target_link_libraries(${GAME_TARGET} PRIVATE
    nt_core nt_app nt_input nt_window nt_gfx nt_text_renderer nt_font
    nt_resource nt_material nt_shared nt_render nt_hash nt_fs nt_log nt_math nt_time
    nt_mesh_renderer nt_entity nt_transform_comp nt_mesh_comp nt_material_comp nt_drawable_comp
    nt_ui nt_sprite_renderer nt_atlas nt_mem_scratch nt_clipboard
    glad stb_image_write
)
if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http nt_platform_web)
else()
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http_stub)
endif()
if(GAME_DEVAPI_ENABLED)
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
target_compile_definitions(${GAME_TARGET} PRIVATE
    GAME_ASSET_PACK_PATH="${GAME_ASSETS_DIR}/game.ntpack"
    NT_INTROSPECT_ENABLED=$<BOOL:${NT_INTROSPECT_ENABLED}>
    NT_INTROSPECT_WRITE_ENABLED=$<BOOL:${NT_INTROSPECT_WRITE_ENABLED}>
    _CRT_SECURE_NO_WARNINGS
)
if(NOT GAME_DEVAPI_ENABLED)
    target_compile_definitions(${GAME_TARGET} PRIVATE NT_DEVAPI_ENABLED=0)
endif()
nt_set_warning_flags(${GAME_TARGET})
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

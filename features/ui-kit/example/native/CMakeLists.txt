cmake_minimum_required(VERSION 3.25)
project(ui_lab LANGUAGES C)

# A standalone consumer of the studio UI kit: engine + kit + this example's own
# scenes, nothing from the game template. It exists to try components and
# themes in the real renderer and to be copied from, one screen at a time.

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(GAME_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(ENGINE_DIR "${GAME_REPO_ROOT}/external/neotolis-engine")
if(NOT EXISTS "${ENGINE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "engine checkout not found at ${ENGINE_DIR}")
endif()
# Native only: the pack comes from the native builder and there is no web shell.
if(EMSCRIPTEN)
    message(FATAL_ERROR "ui_lab is a native example; a web build is not wired")
endif()

# The engine places its archives under build/engine/<preset>; two build types
# sharing one name would overwrite each other's libraries.
if(NOT NT_PRESET_NAME)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(NT_PRESET_NAME "native-release")
    else()
        set(NT_PRESET_NAME "native-debug")
    endif()
endif()

# The kit's on_world role outlines its glyphs; the engine builds that path only on request.
set(NT_FONT_EMBOLDEN_ENABLED ON CACHE BOOL "Glyph outline and synthetic weight for the kit's outlined labels." FORCE)

add_subdirectory("${ENGINE_DIR}" "${CMAKE_BINARY_DIR}/_engine")
if(NOT TARGET nt_builder)
    add_subdirectory("${ENGINE_DIR}/tools/builder" "${CMAKE_BINARY_DIR}/_builder")
    # The pack builder is an offline tool; -O2 keeps a Debug tree from encoding
    # for minutes.
    foreach(nt_builder_opt_tgt nt_basisu_encoder clipper2 nt_clipper2_bridge tinycthread nt_builder builder)
        if(TARGET ${nt_builder_opt_tgt})
            target_compile_options(${nt_builder_opt_tgt} PRIVATE $<$<NOT:$<CONFIG:Release>>:-O2>)
        endif()
    endforeach()
endif()

set(UI_KIT_DIR "${GAME_REPO_ROOT}/features/ui-kit")
set(UI_KIT_INC "${UI_KIT_DIR}/include")
set(UI_KIT_SRC "${UI_KIT_DIR}/src")

set(LAB_TARGET ui_lab)
set(LAB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(LAB_PACK_DIR "${CMAKE_BINARY_DIR}/pack")
set(LAB_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(LAB_ASSETS_DIR "${LAB_OUTPUT_DIR}/assets")

add_executable(${LAB_TARGET}
    src/main.c
    src/lab_ui_runtime.c
    src/lab_theme.c
    src/lab.c
    src/lab_chrome.c
    src/scenes/scene_hud.c
    src/scenes/scene_upgrade.c
    src/scenes/scene_result.c
    src/scenes/scene_settings.c
    src/scenes/scene_components.c
    src/scenes/scene_themes.c
    "${UI_KIT_SRC}/ui_tokens.c"
    "${UI_KIT_SRC}/ui_theme.c"
    "${UI_KIT_SRC}/ui_metrics.c"
    "${UI_KIT_SRC}/ui_kit.c"
    "${UI_KIT_SRC}/ui_safe_area.c"
    "${UI_KIT_SRC}/ui_reach.c")
target_include_directories(${LAB_TARGET} PRIVATE src "${UI_KIT_INC}" "${LAB_GENERATED_DIR}")
target_compile_definitions(${LAB_TARGET} PRIVATE
    FEATURE_UI_KIT=1
    NT_DEVAPI_ENABLED=0
    NT_INTROSPECT_ENABLED=$<BOOL:${NT_INTROSPECT_ENABLED}>
    NT_INTROSPECT_WRITE_ENABLED=$<BOOL:${NT_INTROSPECT_WRITE_ENABLED}>
    UI_LAB_PACK_PATH="assets/ui_lab.ntpack"
    _CRT_SECURE_NO_WARNINGS)
# The engine's own UI example's native link set, minus the debug renderers.
target_link_libraries(${LAB_TARGET} PRIVATE
    nt_core nt_app nt_input nt_window nt_math nt_log nt_time nt_resource
    nt_material nt_shared nt_atlas nt_sprite_renderer nt_text_renderer nt_font
    nt_ui nt_render nt_hash nt_mem_scratch nt_clipboard
    nt_gfx nt_http_stub nt_fs nt_platform nt_basisu_transcoder_stub nt_meshwire_stub)
if(WIN32)
    target_link_libraries(${LAB_TARGET} PRIVATE winmm)
endif()
nt_set_warning_flags(${LAB_TARGET})
nt_set_sanitizer_flags(${LAB_TARGET})
set_target_properties(${LAB_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${LAB_OUTPUT_DIR}")

# --- asset pack: kit art for every theme, demo icons, the scene backdrop, the font ---
file(GLOB_RECURSE LAB_PACK_SOURCE_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png")

add_executable(build_ui_lab_packs src/build_packs.c)
target_link_libraries(build_ui_lab_packs PRIVATE nt_builder nt_log nt_meshwire)
target_compile_definitions(build_ui_lab_packs PRIVATE _CRT_SECURE_NO_WARNINGS)
nt_set_static_crt(build_ui_lab_packs)
nt_set_warning_flags(build_ui_lab_packs)
nt_set_sanitizer_flags(build_ui_lab_packs)
set_target_properties(build_ui_lab_packs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${LAB_OUTPUT_DIR}")

# The builder runs from the engine root so the shader resource ids match the
# ones every other consumer requests; the example's own files are addressed
# absolutely through the third argument.
add_custom_command(
    OUTPUT "${LAB_PACK_DIR}/ui_lab.ntpack" "${LAB_GENERATED_DIR}/ui_lab_assets.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LAB_PACK_DIR}" "${LAB_GENERATED_DIR}"
    COMMAND $<TARGET_FILE:build_ui_lab_packs>
        "${LAB_PACK_DIR}" "${LAB_GENERATED_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS build_ui_lab_packs src/build_packs.c ${LAB_PACK_SOURCE_ASSETS}
    WORKING_DIRECTORY "${ENGINE_DIR}"
    COMMENT "Building ui_lab asset pack"
    VERBATIM)
add_custom_command(
    OUTPUT "${LAB_ASSETS_DIR}/ui_lab.ntpack"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LAB_ASSETS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LAB_PACK_DIR}/ui_lab.ntpack" "${LAB_ASSETS_DIR}/ui_lab.ntpack"
    DEPENDS "${LAB_PACK_DIR}/ui_lab.ntpack"
    COMMENT "Copying ui_lab.ntpack -> bin/assets/"
    VERBATIM)
add_custom_target(ui_lab_packs DEPENDS "${LAB_ASSETS_DIR}/ui_lab.ntpack")
add_dependencies(${LAB_TARGET} ui_lab_packs)

cmake_minimum_required(VERSION 3.25)
project(game_seed LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

option(NT_NO_WERROR "Disable -Werror for Neotolis targets." OFF)
option(NT_WASM_QA "Build a faster release-like WASM QA tier." OFF)

set(GAME_DEVAPI_DEFAULT OFF)
if(NOT EMSCRIPTEN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(GAME_DEVAPI_DEFAULT ON)
endif()
option(GAME_DEVAPI_ENABLED "Enable game-side DevAPI command bus for agent playtests." ${GAME_DEVAPI_DEFAULT})

# Engine native DevAPI: enable on the same gate as the game DevAPI so the engine
# builds nt_devapi + nt_devapi_net. This must precede add_subdirectory.
if(GAME_DEVAPI_ENABLED)
    set(NT_DEVAPI_ENABLED ON CACHE BOOL "" FORCE)
endif()

if(EMSCRIPTEN)
    if(NT_WASM_QA)
        set(_nt_wasm_tier "qa")
    elseif(NT_WASM_ANALYSIS)
        set(_nt_wasm_tier "analysis")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(_nt_wasm_tier "release")
    else()
        set(_nt_wasm_tier "debug")
    endif()

    set(_nt_wasm_mode_suffix "")
    if(NT_WASM_ENABLE_SIMD)
        set(_nt_wasm_mode_suffix "-simd")
    elseif(NT_WASM_SIMD_WASM_PATH)
        set(_nt_wasm_mode_suffix "-paired")
    endif()

    set(NT_PRESET_NAME "wasm-${_nt_wasm_tier}${_nt_wasm_mode_suffix}")
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(NT_PRESET_NAME "native-release")
    else()
        set(NT_PRESET_NAME "native-debug")
    endif()
endif()

add_subdirectory(external/neotolis-engine)
if(NOT EMSCRIPTEN AND NOT TARGET nt_builder)
    add_subdirectory(external/neotolis-engine/tools/builder external/neotolis-engine/tools/builder)
endif()

set(GAME_TARGET game_seed)
set(GAME_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/${GAME_TARGET}/${NT_PRESET_NAME}")

add_library(game_cjson STATIC
    external/cjson/cJSON.c
    external/cjson/cJSON.h
)
target_include_directories(game_cjson SYSTEM PUBLIC external/cjson)

set(GAME_STATE_SCHEMA "${CMAKE_SOURCE_DIR}/state/game_state.schema.json")
set(GAME_STATE_CODEGEN "${CMAKE_SOURCE_DIR}/tools/state_codegen/generate_state.py")
set(GAME_STATE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(GAME_STATE_GENERATED_HEADER "${GAME_STATE_GENERATED_DIR}/game_state.h")
set(GAME_STATE_GENERATED_SOURCE "${GAME_STATE_GENERATED_DIR}/game_state.c")
set(GAME_STATE_GENERATED_DEVAPI_SOURCE "${GAME_STATE_GENERATED_DIR}/game_state_devapi.c")
set(GAME_STATE_GENERATED_SCHEMA_HEADER "${GAME_STATE_GENERATED_DIR}/game_state_schema.gen.h")

add_custom_command(
    OUTPUT
        "${GAME_STATE_GENERATED_HEADER}"
        "${GAME_STATE_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_DEVAPI_SOURCE}"
        "${GAME_STATE_GENERATED_SCHEMA_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_CODEGEN}"
        --schema "${GAME_STATE_SCHEMA}"
        --out-dir "${GAME_STATE_GENERATED_DIR}"
    DEPENDS
        "${GAME_STATE_SCHEMA}"
        "${GAME_STATE_CODEGEN}"
        "${CMAKE_SOURCE_DIR}/tools/state_codegen/game_state.c.in"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating game state sources"
    VERBATIM
)
add_custom_target(game_state_codegen
    DEPENDS
        "${GAME_STATE_GENERATED_HEADER}"
        "${GAME_STATE_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_DEVAPI_SOURCE}"
        "${GAME_STATE_GENERATED_SCHEMA_HEADER}"
)

add_library(game_state STATIC
    src/game_storage.c
    src/game_storage.h
    "${GAME_STATE_GENERATED_SOURCE}"
    "${GAME_STATE_GENERATED_HEADER}"
    "${GAME_STATE_GENERATED_SCHEMA_HEADER}"
    "${GAME_STATE_GENERATED_DEVAPI_SOURCE}"
    state/migrations/v0_to_v1.c
)
add_dependencies(game_state game_state_codegen)
target_include_directories(game_state PUBLIC "${GAME_STATE_GENERATED_DIR}" src external/cjson)
target_link_libraries(game_state PUBLIC game_cjson)
if(GAME_DEVAPI_ENABLED)
    target_link_libraries(game_state PUBLIC nt_devapi)
else()
    target_compile_definitions(game_state PUBLIC NT_DEVAPI_ENABLED=0)
endif()
target_compile_definitions(game_state PRIVATE _CRT_SECURE_NO_WARNINGS)
nt_set_warning_flags(game_state)
nt_set_sanitizer_flags(game_state)

set(SKELETAL_ANIMATION_OZZ_SOURCE_DIR "" CACHE PATH "Optional ozz-animation source checkout for reusable skeletal animation extension probes.")
if(NOT EMSCRIPTEN AND SKELETAL_ANIMATION_OZZ_SOURCE_DIR)
    enable_language(CXX)
    add_subdirectory(extensions/skeletal_animation)
    if(WIN32)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
    endif()
endif()

# --- Little Lives asset pack (UI font + Slug text shaders) ---
# Native builder runs at build time, producing the pack + generated asset header.
set(LL_PACK_DIR "${CMAKE_SOURCE_DIR}/assets/packs")
set(LL_PACK_HDR "${CMAKE_BINARY_DIR}/pack_generated")
set(LL_PACK_FILE "${LL_PACK_DIR}/little_lives.ntpack")
set(LL_PACK_HEADER "${LL_PACK_HDR}/little_lives_assets.h")
if(NOT EMSCRIPTEN)
    add_executable(little_lives_build_packs src/build_packs.c)
    target_link_libraries(little_lives_build_packs PRIVATE nt_builder nt_log)
    target_compile_definitions(little_lives_build_packs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(little_lives_build_packs PRIVATE -U_DLL)
    add_custom_command(
        OUTPUT "${LL_PACK_FILE}" "${LL_PACK_HEADER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LL_PACK_DIR}" "${LL_PACK_HDR}"
        COMMAND "$<TARGET_FILE:little_lives_build_packs>" "${LL_PACK_DIR}" "${LL_PACK_HDR}"
        DEPENDS little_lives_build_packs assets/shaders/slug_text.vert assets/shaders/slug_text.frag assets/fonts/LilitaOne.ttf
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building little_lives.ntpack (UI font + text shaders)"
    )
    add_custom_target(little_lives_packs DEPENDS "${LL_PACK_FILE}" "${LL_PACK_HEADER}")
endif()

add_executable(${GAME_TARGET}
    src/clean_seed_main.c
    src/ll_fx.c
    src/ll_fx.h
    src/game_devapi_ui.c
    src/game_devapi_ui.h
    src/game_audio.c
    src/game_audio.h
)
target_link_libraries(${GAME_TARGET} PRIVATE
    nt_core
    nt_app
    nt_input
    nt_window
    nt_gfx
    nt_shape_renderer
    nt_mesh_renderer
    nt_ui
    nt_text_renderer
    nt_sprite_renderer
    nt_atlas
    nt_sprite_comp
    nt_font
    nt_resource
    nt_material
    nt_shared
    nt_entity
    nt_transform_comp
    nt_mesh_comp
    nt_material_comp
    nt_drawable_comp
    nt_render
    nt_hash
    nt_fs
    nt_log
    nt_math
    nt_time
    glad
    stb_image_write
    game_state
    game_cjson
)
if(GAME_DEVAPI_ENABLED)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_devapi nt_devapi_net)
else()
    target_compile_definitions(${GAME_TARGET} PRIVATE NT_DEVAPI_ENABLED=0)
endif()
if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http)
else()
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http_stub)
endif()
target_include_directories(${GAME_TARGET} PRIVATE
    "${GAME_STATE_GENERATED_DIR}"
    "${LL_PACK_HDR}"
    src
    external/cjson
    external/neotolis-engine/deps/glfw/deps
)
target_compile_definitions(${GAME_TARGET} PRIVATE NT_DEVAPI_ENABLED=$<BOOL:${GAME_DEVAPI_ENABLED}> _CRT_SECURE_NO_WARNINGS)
if(NOT EMSCRIPTEN)
    add_dependencies(${GAME_TARGET} little_lives_packs)
    target_compile_definitions(${GAME_TARGET} PRIVATE LL_HAVE_TEXT=1)
endif()
if(TARGET skeletal_animation_extension)
    target_link_libraries(${GAME_TARGET} PRIVATE skeletal_animation_extension)
    target_compile_definitions(${GAME_TARGET} PRIVATE SKELETAL_ANIMATION_EXTENSION_ENABLED=1)
    set_property(TARGET ${GAME_TARGET} PROPERTY LINKER_LANGUAGE CXX)
else()
    target_compile_definitions(${GAME_TARGET} PRIVATE SKELETAL_ANIMATION_EXTENSION_ENABLED=0)
endif()

if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_platform_web)
endif()
if(WIN32 AND NOT EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE winmm)
endif()

nt_set_warning_flags(${GAME_TARGET})
nt_set_sanitizer_flags(${GAME_TARGET})

set_target_properties(${GAME_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}"
)

if(EMSCRIPTEN)
    set_target_properties(${GAME_TARGET} PROPERTIES SUFFIX ".html" OUTPUT_NAME "index")
    nt_configure_shell(${GAME_TARGET}
        TITLE "Game Seed"
        SIMD_WASM_PATH "${NT_WASM_SIMD_WASM_PATH}"
    )
    target_link_options(${GAME_TARGET} PRIVATE
        "SHELL:-sUSE_WEBGL2=1"
        "SHELL:-sMAX_WEBGL_VERSION=2"
        "SHELL:-sMIN_WEBGL_VERSION=2"
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(${GAME_TARGET} PRIVATE
            "SHELL:-sASSERTIONS=2"
            "SHELL:-gsource-map"
        )
    endif()
    if(NT_WASM_ANALYSIS)
        target_link_options(${GAME_TARGET} PRIVATE "SHELL:--profiling-funcs")
    endif()
endif()

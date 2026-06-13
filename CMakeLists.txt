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
set(GAME_STATE_GENERATED_HEADER "${CMAKE_SOURCE_DIR}/src/generated/game_state.h")
set(GAME_STATE_GENERATED_SOURCE "${CMAKE_SOURCE_DIR}/src/generated/game_state.c")
set(GAME_STATE_GENERATED_DEVAPI_SOURCE "${CMAKE_SOURCE_DIR}/src/generated/game_state_devapi.c")
set(GAME_STATE_GENERATED_SCHEMA_HEADER "${CMAKE_SOURCE_DIR}/src/generated/game_state_schema.gen.h")

add_custom_command(
    OUTPUT
        "${GAME_STATE_GENERATED_HEADER}"
        "${GAME_STATE_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_DEVAPI_SOURCE}"
        "${GAME_STATE_GENERATED_SCHEMA_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_CODEGEN}"
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
    src/game_state_actions.c
    src/game_state_actions.h
    "${GAME_STATE_GENERATED_SOURCE}"
    "${GAME_STATE_GENERATED_HEADER}"
    "${GAME_STATE_GENERATED_SCHEMA_HEADER}"
    state/migrations/v0_to_v1.c
)
add_dependencies(game_state game_state_codegen)
target_include_directories(game_state PUBLIC src external/cjson)
target_link_libraries(game_state PUBLIC game_cjson)
target_compile_definitions(game_state PUBLIC NT_DEVAPI_ENABLED=$<BOOL:${GAME_DEVAPI_ENABLED}> PRIVATE _CRT_SECURE_NO_WARNINGS)
nt_set_warning_flags(game_state)
nt_set_sanitizer_flags(game_state)

if(GAME_DEVAPI_ENABLED)
    add_library(game_devapi STATIC
        src/devapi/nt_devapi.c
        src/devapi/nt_devapi_net.c
        src/devapi/nt_devapi.h
        "${GAME_STATE_GENERATED_DEVAPI_SOURCE}"
    )
    target_include_directories(game_devapi PUBLIC src external/cjson)
    target_link_libraries(game_devapi
        PUBLIC
            nt_input
            nt_entity
            nt_transform_comp
            nt_drawable_comp
            game_cjson
            game_state
        PRIVATE
            nt_log
    )
    target_compile_definitions(game_devapi PUBLIC NT_DEVAPI_ENABLED=1 PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(WIN32)
        target_link_libraries(game_devapi PRIVATE ws2_32)
    endif()
    nt_set_warning_flags(game_devapi)
    nt_set_sanitizer_flags(game_devapi)
else()
    add_library(game_devapi INTERFACE)
    target_include_directories(game_devapi INTERFACE src external/cjson)
    target_compile_definitions(game_devapi INTERFACE NT_DEVAPI_ENABLED=0)
endif()

set(GAME_WINDOWS_RESOURCE "")
if(WIN32 AND NOT EMSCRIPTEN)
    enable_language(RC)
    set(GAME_WINDOWS_RESOURCE src/windows/67_world.rc)
    set_source_files_properties(src/windows/67_world.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/runtime/67-world/67-world.ico"
    )
endif()

add_executable(${GAME_TARGET}
    src/main.c
    src/game_audio.c
    src/game_audio.h
    ${GAME_WINDOWS_RESOURCE}
)
target_link_libraries(${GAME_TARGET} PRIVATE
    nt_core
    nt_app
    nt_input
    nt_window
    nt_gfx
    nt_shape_renderer
    nt_sprite_renderer
    nt_resource
    nt_atlas
    nt_material
    nt_hash
    nt_fs
    nt_log
    nt_math
    game_devapi
    game_state
    game_cjson
)
if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http)
else()
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http_stub)
endif()
target_include_directories(${GAME_TARGET} PRIVATE
    src
    src/generated/assets
    external/cjson
    external/neotolis-engine/deps/glfw/deps
)
target_compile_definitions(${GAME_TARGET} PRIVATE NT_DEVAPI_ENABLED=$<BOOL:${GAME_DEVAPI_ENABLED}> _CRT_SECURE_NO_WARNINGS)

if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_platform_web)
endif()
if(WIN32 AND NOT EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE winmm)
endif()

nt_set_warning_flags(${GAME_TARGET})
nt_set_sanitizer_flags(${GAME_TARGET})

if(NOT EMSCRIPTEN)
    add_executable(build_67_world_packs tools/assets/build_67_world_packs.c)
    target_link_libraries(build_67_world_packs PRIVATE nt_builder)
    target_include_directories(build_67_world_packs PRIVATE
        external/neotolis-engine/tools/builder
        external/neotolis-engine/engine
        external/neotolis-engine/shared/include
    )
    target_compile_definitions(build_67_world_packs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(build_67_world_packs PRIVATE -U_DLL)
    nt_set_warning_flags(build_67_world_packs)
    nt_set_sanitizer_flags(build_67_world_packs)
    set_target_properties(build_67_world_packs PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}"
    )
endif()

set_target_properties(${GAME_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}"
)

set(GAME_WORLD67_PACK_SOURCE "${CMAKE_SOURCE_DIR}/build/game_seed/67-world-packs/world67_art.ntpack")
set(GAME_WORLD67_PACK_OUTPUT "${GAME_OUTPUT_DIR}/assets/world67_art.ntpack")
if(EXISTS "${GAME_WORLD67_PACK_SOURCE}")
    add_custom_command(
        OUTPUT "${GAME_WORLD67_PACK_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_OUTPUT_DIR}/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GAME_WORLD67_PACK_SOURCE}" "${GAME_WORLD67_PACK_OUTPUT}"
        DEPENDS "${GAME_WORLD67_PACK_SOURCE}"
        COMMENT "Copying world67_art.ntpack -> native assets/"
        VERBATIM
    )
    add_custom_target(game_world67_pack_copy DEPENDS "${GAME_WORLD67_PACK_OUTPUT}")
    add_dependencies(${GAME_TARGET} game_world67_pack_copy)
else()
    message(WARNING
        "67 World art pack not found at ${GAME_WORLD67_PACK_SOURCE}. "
        "Run build_67_world_packs before packaging; native dev can still load from build/game_seed/67-world-packs when present.")
endif()

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

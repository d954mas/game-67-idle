cmake_minimum_required(VERSION 3.25)
project(game_67_idle LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

option(NT_NO_WERROR "Disable -Werror for Neotolis targets." OFF)
option(GAME_ENABLE_BASISU_RUNTIME "Link Basis Universal runtime transcoder into the game executable." OFF)
set(NT_BASISU ${GAME_ENABLE_BASISU_RUNTIME} CACHE BOOL "Link Basis Universal transcoder for GPU texture compression" FORCE)

set(GAME_DEVAPI_DEFAULT OFF)
if(NOT EMSCRIPTEN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(GAME_DEVAPI_DEFAULT ON)
endif()
option(GAME_DEVAPI_ENABLED "Enable temporary game-side devapi TCP command bus." ${GAME_DEVAPI_DEFAULT})

if(EMSCRIPTEN)
    if(NT_WASM_ANALYSIS)
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

set(GAME_TARGET game_67_idle)
set(GAME_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/game_67_idle/${NT_PRESET_NAME}")
set(GAME_PACK_DIR "${CMAKE_SOURCE_DIR}/build/game_67_idle")

add_library(game_cjson STATIC
    external/cjson/cJSON.c
    external/cjson/cJSON.h
)
target_include_directories(game_cjson SYSTEM PUBLIC external/cjson)

set(GAME_STATE_SCHEMA "${CMAKE_SOURCE_DIR}/state/game_state.schema.json")
set(GAME_STATE_CODEGEN "${CMAKE_SOURCE_DIR}/tools/state_codegen/generate_state.py")
set(GAME_STATE_SOURCE_TEMPLATE "${CMAKE_SOURCE_DIR}/tools/state_codegen/game_state.c.in")
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
        "${GAME_STATE_SOURCE_TEMPLATE}"
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
    src/generated/game_state.h
    src/generated/game_state_schema.gen.h
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
    target_include_directories(game_devapi PUBLIC src)
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
    target_compile_definitions(game_devapi PUBLIC NT_DEVAPI_ENABLED=1)
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

add_executable(${GAME_TARGET} src/main.c)
target_link_libraries(${GAME_TARGET} PRIVATE
    nt_core
    nt_app
    nt_input
    nt_window
    nt_gfx
    nt_shape_renderer
    nt_math
    nt_entity
    nt_transform_comp
    nt_drawable_comp
    game_devapi
    game_state
)

if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_platform_web)
endif()

nt_set_warning_flags(${GAME_TARGET})
nt_set_sanitizer_flags(${GAME_TARGET})

set_target_properties(${GAME_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}"
)

if(NOT EMSCRIPTEN)
    add_subdirectory(external/neotolis-engine/tools/builder)

    add_executable(build_game_67_idle_pack src/build_packs.c)
    target_link_libraries(build_game_67_idle_pack PRIVATE nt_builder)
    target_compile_definitions(build_game_67_idle_pack PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(build_game_67_idle_pack PRIVATE -U_DLL)
    nt_set_warning_flags(build_game_67_idle_pack)
    nt_set_sanitizer_flags(build_game_67_idle_pack)
    set_target_properties(build_game_67_idle_pack PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/tools/builder/${NT_PRESET_NAME}"
    )

    add_custom_target(game_67_idle_pack
        COMMAND $<TARGET_FILE:build_game_67_idle_pack> "${GAME_PACK_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS build_game_67_idle_pack
        COMMENT "Building game_67_idle.ntpack"
        VERBATIM
    )
endif()

if(EMSCRIPTEN)
    set_target_properties(${GAME_TARGET} PROPERTIES SUFFIX ".html" OUTPUT_NAME "index")
    nt_configure_shell(${GAME_TARGET}
        TITLE "Game 67 Idle - Neotolis Engine"
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

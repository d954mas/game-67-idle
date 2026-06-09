cmake_minimum_required(VERSION 3.25)
project(game_67_idle LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(NT_NO_WERROR "Disable -Werror for Neotolis targets." OFF)
option(GAME_ENABLE_BASISU_RUNTIME "Link Basis Universal runtime transcoder into the game executable." OFF)
set(NT_BASISU ${GAME_ENABLE_BASISU_RUNTIME} CACHE BOOL "Link Basis Universal transcoder for GPU texture compression" FORCE)

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

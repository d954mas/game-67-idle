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
    state/migrations/v0_to_v1.c
)
add_dependencies(game_state game_state_codegen)
target_include_directories(game_state PUBLIC "${GAME_STATE_GENERATED_DIR}" src external/cjson)
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
    target_include_directories(game_devapi PUBLIC "${GAME_STATE_GENERATED_DIR}" src external/cjson)
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
    target_include_directories(game_devapi INTERFACE "${GAME_STATE_GENERATED_DIR}" src external/cjson)
    target_compile_definitions(game_devapi INTERFACE NT_DEVAPI_ENABLED=0)
endif()

# ---- Critter Corral sprite pack ----
# Real sprite visuals are drawn through nt_sprite_renderer + nt_atlas from an
# offline-built .ntpack. The pack builder is native-only (nt_builder). The pack
# is written to a tracked runtime location and mounted at that relative path by
# the game at startup. Pack building is explicit: a missing pack downgrades the
# game to a procedural fallback (see clean_seed_main.c) instead of failing the
# build, matching the bunnymark "packer is invoked manually" pattern.
set(CORRAL_PACK_DIR "${CMAKE_SOURCE_DIR}/assets/runtime/critter-corral")
set(CORRAL_PACK "${CORRAL_PACK_DIR}/critter_corral.ntpack")
set(CORRAL_GENERATED_DIR "${CMAKE_SOURCE_DIR}/tools/critter_corral/generated")
set(CORRAL_SPRITE_SCRIPT "${CMAKE_SOURCE_DIR}/tools/critter_corral/generate_sprites.py")
set(CORRAL_SPRITE_DIR "${CMAKE_SOURCE_DIR}/gamedesign/projects/critter-corral/art/sprites")

if(NOT EMSCRIPTEN AND NOT TARGET build_critter_corral_packs)
    add_executable(build_critter_corral_packs tools/critter_corral/build_packs.c)
    target_link_libraries(build_critter_corral_packs PRIVATE nt_builder)
    target_compile_definitions(build_critter_corral_packs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(build_critter_corral_packs PRIVATE -U_DLL)
    nt_set_warning_flags(build_critter_corral_packs)
    nt_set_sanitizer_flags(build_critter_corral_packs)

    # (Re)build the pack from the durable source PNGs. Runs from the project
    # root so the builder resolves engine shaders (via asset root) and source
    # PNGs by their project-relative paths. The sprite PNGs are durable source
    # assets; regenerate them with `py -3.12 ${CORRAL_SPRITE_SCRIPT}` (needs
    # PIL+numpy) when art changes. Wired as a custom target the user invokes
    # explicitly (e.g. cmake --build ... --target critter_corral_packs).
    set(CORRAL_SPRITE_PNGS
        "${CORRAL_SPRITE_DIR}/critter_a.png"
        "${CORRAL_SPRITE_DIR}/critter_b.png"
        "${CORRAL_SPRITE_DIR}/critter.png"
        "${CORRAL_SPRITE_DIR}/pen.png"
        "${CORRAL_SPRITE_DIR}/grass.png"
        "${CORRAL_SPRITE_DIR}/lure.png"
        "${CORRAL_SPRITE_DIR}/spark.png"
        "${CORRAL_SPRITE_DIR}/icon_radius.png"
        "${CORRAL_SPRITE_DIR}/icon_pull.png"
        "${CORRAL_SPRITE_DIR}/icon_second_lure.png"
        "${CORRAL_SPRITE_DIR}/icon_gate.png"
        "${CORRAL_SPRITE_DIR}/icon_calm.png"
        "${CORRAL_SPRITE_DIR}/icon_chain.png"
        "${CORRAL_SPRITE_DIR}/card.png"
    )
    add_custom_command(
        OUTPUT "${CORRAL_PACK}"
        COMMAND "$<TARGET_FILE:build_critter_corral_packs>" "${CORRAL_PACK_DIR}"
        DEPENDS
            build_critter_corral_packs
            ${CORRAL_SPRITE_PNGS}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building critter_corral.ntpack from source sprites"
        VERBATIM
    )
    add_custom_target(critter_corral_packs DEPENDS "${CORRAL_PACK}")
endif()

add_executable(${GAME_TARGET}
    src/clean_seed_main.c
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
    "${GAME_STATE_GENERATED_DIR}"
    "${CORRAL_GENERATED_DIR}"
    src
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

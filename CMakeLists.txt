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

# Engine native DevAPI: enable on the same gate as the game DevAPI so the engine
# builds nt_devapi + nt_devapi_net (command groups default ON). Must precede the
# engine add_subdirectory so the option is read.
if(GAME_DEVAPI_ENABLED)
    set(NT_DEVAPI_ENABLED ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(external/neotolis-engine)
if(NOT EMSCRIPTEN AND NOT TARGET nt_builder)
    add_subdirectory(external/neotolis-engine/tools/builder external/neotolis-engine/tools/builder)
endif()

set(GAME_TARGET game_seed)
set(GAME_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/${GAME_TARGET}/${NT_PRESET_NAME}")
set(MECH_BUILDER_PACK_DIR "${CMAKE_SOURCE_DIR}/build/mech-builder-battler")
set(MECH_BUILDER_ASSETS_DIR "${GAME_OUTPUT_DIR}/assets")

# cJSON comes from the engine `cjson` target (single cJSON across the engine bus
# and the game-owned handlers — two cJSON builds would clash on cJSON_* symbols).

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
target_include_directories(game_state PUBLIC "${GAME_STATE_GENERATED_DIR}" src)
target_link_libraries(game_state PUBLIC cjson)
target_compile_definitions(game_state PRIVATE _CRT_SECURE_NO_WARNINGS)
# nt_devapi (engine) provides the command bus + NT_DEVAPI_ENABLED=1 (PUBLIC) for
# the generated game.state.* handlers. When the gate is OFF the engine target is
# absent, so define the macro =0 here (PUBLIC, propagates to game_seed) — the
# `#if NT_DEVAPI_ENABLED` guards need it defined, not undefined (-Werror,-Wundef).
if(GAME_DEVAPI_ENABLED)
    target_link_libraries(game_state PUBLIC nt_devapi)
else()
    target_compile_definitions(game_state PUBLIC NT_DEVAPI_ENABLED=0)
endif()
nt_set_warning_flags(game_state)
nt_set_sanitizer_flags(game_state)

# The DevAPI is now the engine's native nt_devapi + nt_devapi_net (linked into
# game_state/game_seed below); the generated game.state.* handlers compile into
# game_state. No game-side bus/transport lib anymore.

set(SKELETAL_ANIMATION_OZZ_SOURCE_DIR "" CACHE PATH "Optional ozz-animation source checkout for reusable skeletal animation extension probes.")
if(NOT EMSCRIPTEN AND SKELETAL_ANIMATION_OZZ_SOURCE_DIR)
    enable_language(CXX)
    add_subdirectory(extensions/skeletal_animation)
    if(WIN32)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
    endif()
endif()

add_executable(${GAME_TARGET}
    src/clean_seed_main.c
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
    game_state
)
# glad (GL loader) and stb_image_write (PNG writer) are native-only engine deps:
# the engine adds deps/glad + deps/stb only inside if(NOT EMSCRIPTEN), so these
# targets do not exist on wasm. On wasm nt_gfx supplies GL via Emscripten's
# WebGL2/GLES3 layer (gl/nt_gfx_gl_ctx_web.c) and no game source writes PNGs.
if(NOT EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE
        glad
        stb_image_write
    )
endif()
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
    src
    external/neotolis-engine/deps/glfw/deps
)
target_compile_definitions(${GAME_TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
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

if(NOT EMSCRIPTEN)
    add_executable(build_mech_builder_battler_packs
        tools/mech-builder-battler/build_packs.c
    )
    target_link_libraries(build_mech_builder_battler_packs PRIVATE nt_builder nt_log)
    target_compile_definitions(build_mech_builder_battler_packs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(build_mech_builder_battler_packs PRIVATE -U_DLL)
    nt_set_warning_flags(build_mech_builder_battler_packs)
    nt_set_sanitizer_flags(build_mech_builder_battler_packs)
    set_target_properties(build_mech_builder_battler_packs PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}"
    )

    set(MECH_BUILDER_PACK "${MECH_BUILDER_PACK_DIR}/mech_builder_battler_mesh.ntpack")
    set(MECH_BUILDER_PACK_OUTPUT "${MECH_BUILDER_ASSETS_DIR}/mech_builder_battler_mesh.ntpack")
    set(HERO_MECH_SOURCE_ATLAS
        "${CMAKE_SOURCE_DIR}/assets/source/models/quaternius/poly_pizza_quaternius_mech_cc0_atlas.png"
    )
    set(HERO_MECH_TOY_ATLAS
        "${CMAKE_SOURCE_DIR}/assets/textures/poly_pizza_quaternius_mech_toy_atlas.png"
    )
    add_custom_command(
        OUTPUT "${HERO_MECH_TOY_ATLAS}"
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/mech-builder-battler/recolor_mech_atlas.py"
            "${HERO_MECH_SOURCE_ATLAS}"
            "${HERO_MECH_TOY_ATLAS}"
        DEPENDS
            "${HERO_MECH_SOURCE_ATLAS}"
            tools/mech-builder-battler/recolor_mech_atlas.py
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating toy/plastic hero mech atlas from CC0 source atlas"
        VERBATIM
    )
    set(ROBOT_ENEMY_SOURCE_GLB
        "${CMAKE_SOURCE_DIR}/assets/source/models/quaternius/poly_pizza_quaternius_robot_enemy_legs_gun_cc0.glb"
    )
    set(ASSAULT_WALKER_SOURCE_GLB
        "${CMAKE_SOURCE_DIR}/assets/source/models/poly_pizza/alimayo_arango/poly_pizza_alimayo_mech_assault_walker_ccby30.glb"
    )
    set(ROBOT_ENEMY_STATIC_MESHES
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_main2_static_cc0.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_main_static_cc0.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_edge_static_cc0.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_dark_static_cc0.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_eye_static_cc0.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_grey_static_cc0.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_lightgrey_static_cc0.gltf"
    )
    add_custom_command(
        OUTPUT ${ROBOT_ENEMY_STATIC_MESHES}
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/mech-builder-battler/extract_static_gltf.py"
            --split-materials
            "${ROBOT_ENEMY_SOURCE_GLB}"
            "${CMAKE_SOURCE_DIR}/assets/meshes"
            poly_pizza_quaternius_robot_enemy_legs_gun
        DEPENDS
            "${ROBOT_ENEMY_SOURCE_GLB}"
            tools/mech-builder-battler/extract_static_gltf.py
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Extracting static material meshes from CC0 robot enemy GLB"
        VERBATIM
    )
    set(ASSAULT_WALKER_STATIC_MESHES
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_005_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_005_ao_2_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_005_uv_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_006_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_001_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_uv_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_002_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_003_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_004_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_004_none_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_007_static_ccby30.gltf"
        "${CMAKE_SOURCE_DIR}/assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_007_none_static_ccby30.gltf"
    )
    add_custom_command(
        OUTPUT ${ASSAULT_WALKER_STATIC_MESHES}
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/mech-builder-battler/extract_static_gltf.py"
            --split-materials
            "${ASSAULT_WALKER_SOURCE_GLB}"
            "${CMAKE_SOURCE_DIR}/assets/meshes"
            poly_pizza_alimayo_mech_assault_walker
            ccby30
        DEPENDS
            "${ASSAULT_WALKER_SOURCE_GLB}"
            tools/mech-builder-battler/extract_static_gltf.py
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Extracting static material meshes from CC-BY Assault Walker GLB"
        VERBATIM
    )
    target_compile_definitions(${GAME_TARGET} PRIVATE
        "MECH_BUILDER_PACK_PATH=\"${MECH_BUILDER_PACK_OUTPUT}\""
    )
    add_custom_command(
        OUTPUT "${MECH_BUILDER_PACK}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MECH_BUILDER_PACK_DIR}"
        COMMAND $<TARGET_FILE:build_mech_builder_battler_packs> "${MECH_BUILDER_PACK_DIR}"
        DEPENDS
            build_mech_builder_battler_packs
            tools/mech-builder-battler/build_packs.c
            tools/mech-builder-battler/extract_static_gltf.py
            tools/mech-builder-battler/generate_starter_mech_meshes.py
            tools/mech-builder-battler/generate_world_floor_mesh.py
            tools/mech-builder-battler/recolor_mech_atlas.py
            assets/shaders/mech_mesh_inst.vert
            assets/shaders/mech_mesh_inst.frag
            assets/shaders/mech_mesh_color_inst.frag
            assets/shaders/mech_mesh_solid_inst.vert
            assets/shaders/mech_mesh_solid_inst.frag
            assets/shaders/slug_text.vert
            assets/shaders/slug_text.frag
            assets/shaders/common/instance.glsl
            assets/shaders/common/globals.glsl
            external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf
            assets/meshes/mech_starter_torso.gltf
            assets/meshes/mech_starter_pelvis.gltf
            assets/meshes/mech_starter_head.gltf
            assets/meshes/mech_starter_shoulder.gltf
            assets/meshes/mech_starter_limb.gltf
            assets/meshes/mech_starter_forearm.gltf
            assets/meshes/mech_starter_weapon.gltf
            assets/meshes/mech_starter_foot.gltf
            assets/meshes/mech_starter_rocket_pod.gltf
            assets/meshes/mech_starter_rocket_tube.gltf
            assets/meshes/mech_starter_vent.gltf
            assets/meshes/mech_starter_hydraulic.gltf
            assets/meshes/mech_starter_joint.gltf
            assets/meshes/mech_starter_armor_plate.gltf
            assets/meshes/mech_starter_visor.gltf
            assets/meshes/mech_world_studs_floor.gltf
            assets/meshes/poly_pizza_quaternius_mech_cc0.glb
            ${ROBOT_ENEMY_STATIC_MESHES}
            ${ASSAULT_WALKER_STATIC_MESHES}
            "${HERO_MECH_TOY_ATLAS}"
            assets/textures/mech_builder_battler_stylized_studs_grass_tile_v1.png
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building Mech Builder Battler mesh pack"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${MECH_BUILDER_PACK_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MECH_BUILDER_ASSETS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MECH_BUILDER_PACK}" "${MECH_BUILDER_PACK_OUTPUT}"
        DEPENDS "${MECH_BUILDER_PACK}"
        COMMENT "Copying Mech Builder Battler mesh pack -> ${NT_PRESET_NAME}/assets/"
        VERBATIM
    )
    add_custom_target(mech_builder_battler_packs DEPENDS "${MECH_BUILDER_PACK_OUTPUT}")
    add_dependencies(${GAME_TARGET} mech_builder_battler_packs)
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

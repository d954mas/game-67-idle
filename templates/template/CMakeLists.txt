cmake_minimum_required(VERSION 3.25)
project(game LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Two-build convention: the human build (native-debug, VS Code) has no DevAPI;
# agents configure their own dir (build/devapi-debug) with -DGAME_DEVAPI_ENABLED=ON
# and get a distinct window title so both can run side by side.
if(NOT DEFINED GAME_DEVAPI_ENABLED)
    set(GAME_DEVAPI_ENABLED OFF CACHE BOOL "Build the template with engine DevAPI automation support." FORCE)
endif()

# Preset name must encode the devapi split: engine libs land in
# build/engine/<preset> and build/tools/builder/<preset>, so human and agent
# builds sharing one preset would overwrite each other's engine libs
# (nt_input with/without inject symbols) — the exact parallel-build collision
# the convention exists to prevent.
if(EMSCRIPTEN)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(NT_PRESET_NAME "wasm-release")
    elseif(GAME_DEVAPI_ENABLED)
        set(NT_PRESET_NAME "wasm-devapi-debug")
    else()
        set(NT_PRESET_NAME "wasm-debug")
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(NT_PRESET_NAME "native-release")
elseif(GAME_DEVAPI_ENABLED)
    set(NT_PRESET_NAME "devapi-debug")
else()
    set(NT_PRESET_NAME "native-debug")
endif()
set(GAME_TITLE "Template" CACHE STRING "Game window title base")
if(GAME_DEVAPI_ENABLED)
    set(GAME_WINDOW_TITLE "${GAME_TITLE} [AI]")
else()
    set(GAME_WINDOW_TITLE "${GAME_TITLE}")
endif()
if(NOT DEFINED FEATURE_GAME_STATE)
    set(FEATURE_GAME_STATE ON CACHE BOOL "Build the installed game-state feature." FORCE)
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
    # The pack builder is an offline tool: force -O2 even in Debug game trees
    # (basisu at -O0 encodes for minutes, not seconds). Engine CI does the same
    # by building its pack builders with a release preset; games get the same
    # effect here without patching the engine. Asserts stay on (no NDEBUG).
    foreach(nt_builder_opt_tgt nt_basisu_encoder clipper2 nt_clipper2_bridge tinycthread nt_builder builder)
        target_compile_options(${nt_builder_opt_tgt} PRIVATE $<$<NOT:$<CONFIG:Release>>:-O2>)
    endforeach()
endif()

set(GAME_TARGET game)
set(GAME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(GAME_PACK_DIR "${CMAKE_BINARY_DIR}/pack")
set(GAME_ASSETS_DIR "${GAME_OUTPUT_DIR}/assets")
set(GAME_SOURCE_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")

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
if(FEATURE_GAME_STATE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(GAME_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/game_state.schema.json")
    set(GAME_STATE_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/generate_state.py")
    set(GAME_STATE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/game-state")
    set(GAME_STATE_GENERATED_HEADER "${GAME_STATE_GENERATED_DIR}/game_state.h")
    set(GAME_STATE_GENERATED_SOURCE "${GAME_STATE_GENERATED_DIR}/game_state.c")
    set(GAME_STATE_GENERATED_SCHEMA "${GAME_STATE_GENERATED_DIR}/game_state_schema.gen.h")
    set(GAME_STATE_GENERATED_DEVAPI "${GAME_STATE_GENERATED_DIR}/game_state_devapi.c")
    add_custom_command(
        OUTPUT
            "${GAME_STATE_GENERATED_HEADER}"
            "${GAME_STATE_GENERATED_SOURCE}"
            "${GAME_STATE_GENERATED_SCHEMA}"
            "${GAME_STATE_GENERATED_DEVAPI}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_STATE_GENERATED_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_GENERATOR}"
            --schema "${GAME_STATE_SCHEMA}"
            --out-dir "${GAME_STATE_GENERATED_DIR}"
        DEPENDS
            "${GAME_STATE_SCHEMA}"
            "${GAME_STATE_GENERATOR}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        COMMENT "Generating installed game-state feature sources"
        VERBATIM
    )
    target_sources(${GAME_TARGET} PRIVATE
        "${GAME_STATE_GENERATED_SOURCE}"
        src/game_state_json.c
        src/game_storage.c
        src/game_save.c              # A3
        src/game_fragment.c          # A3
        state/migrations/v0_to_v1.c
    )
    target_link_libraries(${GAME_TARGET} PRIVATE cjson)
    target_compile_definitions(${GAME_TARGET} PRIVATE FEATURE_GAME_STATE=1)
    target_compile_definitions(${GAME_TARGET} PRIVATE
        GAME_SAVE_AUTOSAVE_SLOT="autosave"
        GAME_SAVE_DEBOUNCE_MS=2000
        GAME_SAVE_MAX_INTERVAL_MS=30000
        GAME_SAVE_DOC_VERSION=1
    )
    target_include_directories(${GAME_TARGET} PRIVATE "${GAME_STATE_GENERATED_DIR}")
    if(GAME_DEVAPI_ENABLED)
        target_sources(${GAME_TARGET} PRIVATE "${GAME_STATE_GENERATED_DEVAPI}")
    endif()
else()
    target_compile_definitions(${GAME_TARGET} PRIVATE FEATURE_GAME_STATE=0)
endif()
target_include_directories(${GAME_TARGET} PRIVATE src "${GAME_SOURCE_GENERATED_DIR}" "${ENGINE_DIR}/deps/glfw/deps")
target_compile_definitions(${GAME_TARGET} PRIVATE GAME_WINDOW_TITLE="${GAME_WINDOW_TITLE}")
target_link_libraries(${GAME_TARGET} PRIVATE
    nt_core nt_app nt_input nt_window nt_gfx nt_text_renderer nt_font
    nt_resource nt_material nt_shared nt_render nt_hash nt_fs nt_log nt_math nt_time
    nt_mesh_renderer nt_entity nt_transform_comp nt_mesh_comp nt_material_comp nt_drawable_comp
    nt_ui nt_sprite_renderer nt_atlas nt_mem_scratch nt_clipboard
)
if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http nt_platform_web)
    target_link_options(${GAME_TARGET} PRIVATE
        "SHELL:-sFORCE_FILESYSTEM=1"
        "SHELL:-sEXPORTED_FUNCTIONS=_main,_malloc,_free"
        "SHELL:-sMIN_WEBGL_VERSION=2"
        "SHELL:-sMAX_WEBGL_VERSION=2"
        "SHELL:-sFULL_ES3=1"
        # Engine targets set memory flags PRIVATE on static libs, which never
        # reach the executable link: without these the heap is fixed at 16MB
        # and texture transcode OOMs (Aborted(OOM) on web).
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sINITIAL_MEMORY=64MB"
    )
else()
    # glad/stb_image_write are native-only (no GL loader / file dump on web).
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http_stub glad stb_image_write)
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
# GAME_STORAGE_APP_ID namespaces the shared web-origin (itch) localStorage; "template" is
# the template's own default, games override it (A2).
target_compile_definitions(${GAME_TARGET} PRIVATE GAME_STORAGE_APP_ID="template")
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

# --- native C unit tests (Unity + CTest); introduced in A1, extended in A2/A3 ---
if(NOT EMSCRIPTEN)
    enable_testing()
    add_executable(test_game_state_json tests/test_game_state_json.c src/game_state_json.c)
    target_link_libraries(test_game_state_json PRIVATE cjson unity)
    target_include_directories(test_game_state_json PRIVATE src)
    target_compile_definitions(test_game_state_json PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_state_json PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_state_json COMMAND test_game_state_json)

    add_executable(test_game_storage tests/test_game_storage.c src/game_storage.c)
    target_link_libraries(test_game_storage PRIVATE unity)
    target_include_directories(test_game_storage PRIVATE src)
    target_compile_definitions(test_game_storage PRIVATE
        GAME_STORAGE_APP_ID="template_test" _CRT_SECURE_NO_WARNINGS)
    # MoveFileExA (native quarantine/atomic-replace) is in kernel32, linked by default.
    set_target_properties(test_game_storage PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_storage COMMAND test_game_storage
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    add_executable(test_game_save
        tests/test_game_save.c
        src/game_save.c src/game_storage.c src/game_state_json.c)
    target_link_libraries(test_game_save PRIVATE cjson unity)
    target_include_directories(test_game_save PRIVATE src)
    target_compile_definitions(test_game_save PRIVATE
        GAME_SAVE_TESTING=1
        GAME_STORAGE_APP_ID="template_test"
        GAME_SAVE_AUTOSAVE_SLOT="test_slot"
        GAME_SAVE_DEBOUNCE_MS=2000
        GAME_SAVE_MAX_INTERVAL_MS=30000
        GAME_SAVE_DOC_VERSION=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_save PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_save COMMAND test_game_save
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

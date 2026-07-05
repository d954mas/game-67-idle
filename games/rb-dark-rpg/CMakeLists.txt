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
if(NOT DEFINED FEATURE_GAME_STATE)
    set(FEATURE_GAME_STATE ON CACHE BOOL "Build the installed game-state feature." FORCE)
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)

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
set(GAME_AUDIO_ASSETS_DIR "${GAME_ASSETS_DIR}/audio")
set(GAME_SOURCE_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")
set(GAME_CONTENT_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_dialogue_content.py")
set(GAME_CONTENT_COMPATIBILITY_VALIDATOR "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_content_compatibility.py")
set(GAME_CONTENT_COMPATIBILITY "${CMAKE_CURRENT_SOURCE_DIR}/design/data/content_compatibility.json")
set(GAME_CONTENT_DIALOGUES "${CMAKE_CURRENT_SOURCE_DIR}/design/data/dialogues.json")
set(GAME_CONTENT_CHARACTERS "${CMAKE_CURRENT_SOURCE_DIR}/design/data/characters.json")
set(GAME_CONTENT_QUESTS "${CMAKE_CURRENT_SOURCE_DIR}/design/data/quests.json")
set(GAME_CONTENT_ITEMS "${CMAKE_CURRENT_SOURCE_DIR}/design/data/items.json")
set(GAME_CONTENT_LOCATIONS "${CMAKE_CURRENT_SOURCE_DIR}/design/data/locations.json")
set(GAME_CONTENT_SERVICES "${CMAKE_CURRENT_SOURCE_DIR}/design/data/services.json")
set(GAME_CONTENT_COMBAT "${CMAKE_CURRENT_SOURCE_DIR}/design/data/combat.json")
set(GAME_CONTENT_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/design/data/content_manifest.json")
set(GAME_STATE_SCHEMA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/state/game_state.schema.json")
set(GAME_CONTENT_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/content")
set(GAME_CONTENT_GENERATED_SOURCE "${GAME_CONTENT_GENERATED_DIR}/game_content.generated.c")

add_custom_target(content_compatibility_check
    COMMAND "${Python3_EXECUTABLE}" "${GAME_CONTENT_COMPATIBILITY_VALIDATOR}"
        --game-dir "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS
        "${GAME_CONTENT_COMPATIBILITY_VALIDATOR}"
        "${GAME_CONTENT_COMPATIBILITY}"
        "${GAME_CONTENT_MANIFEST}"
        "${GAME_CONTENT_DIALOGUES}"
        "${GAME_CONTENT_CHARACTERS}"
        "${GAME_CONTENT_QUESTS}"
        "${GAME_CONTENT_ITEMS}"
        "${GAME_CONTENT_LOCATIONS}"
        "${GAME_CONTENT_SERVICES}"
        "${GAME_CONTENT_COMBAT}"
        "${GAME_STATE_SCHEMA_SOURCE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Validating rb-dark-rpg save-compatible content ids"
    VERBATIM
)

add_custom_command(
    OUTPUT "${GAME_CONTENT_GENERATED_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_CONTENT_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${GAME_CONTENT_GENERATOR}"
        "${GAME_CONTENT_DIALOGUES}"
        "${GAME_CONTENT_CHARACTERS}"
        "${GAME_CONTENT_QUESTS}"
        "${GAME_CONTENT_ITEMS}"
        "${GAME_CONTENT_COMBAT}"
        "${GAME_CONTENT_LOCATIONS}"
        "${GAME_CONTENT_SERVICES}"
        "${GAME_CONTENT_GENERATED_SOURCE}"
    DEPENDS
        "${GAME_CONTENT_GENERATOR}"
        "${GAME_CONTENT_DIALOGUES}"
        "${GAME_CONTENT_CHARACTERS}"
        "${GAME_CONTENT_QUESTS}"
        "${GAME_CONTENT_ITEMS}"
        "${GAME_CONTENT_COMBAT}"
        "${GAME_CONTENT_LOCATIONS}"
        "${GAME_CONTENT_SERVICES}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating rb-dark-rpg dialogue content"
    VERBATIM
)

# --- asset pack builder (runs at build time -> game.ntpack + asset-id header) ---
if(NOT EMSCRIPTEN)
    file(GLOB_RECURSE GAME_AUDIO_ASSET_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/*"
    )

    add_custom_command(
        OUTPUT "${GAME_AUDIO_ASSETS_DIR}/.audio_assets.stamp"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${GAME_AUDIO_ASSETS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_AUDIO_ASSETS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio" "${GAME_AUDIO_ASSETS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E touch "${GAME_AUDIO_ASSETS_DIR}/.audio_assets.stamp"
        DEPENDS ${GAME_AUDIO_ASSET_FILES}
        COMMENT "Copying loose audio assets -> bin/assets/audio"
        VERBATIM
    )
    add_custom_target(game_audio_assets DEPENDS "${GAME_AUDIO_ASSETS_DIR}/.audio_assets.stamp")

    add_executable(build_game_packs src/build_packs.c)
    target_link_libraries(build_game_packs PRIVATE nt_builder nt_log)
    target_compile_definitions(build_game_packs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(build_game_packs PRIVATE -U_DLL)
    set_target_properties(build_game_packs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}")
    set(ASH_BORDER_MAP_PNG "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/world_map_ash_border_01/ash_border_map.png")
    set(GAME_COMBAT_ACTOR_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/combat_actor_sprites_01/slices/combat_actor_hero.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/combat_actor_sprites_01/slices/combat_actor_gate_scavenger.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/combat_actor_sprites_01/slices/combat_actor_mill_scavenger.png"
    )
    set(GAME_LOCATION_SCENE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/scenes/gate_outskirts_background_1280x700.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/scenes/old_mill_background_1280x700.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/location_object_sprites_01/asset_object_black_sun_mark.png"
    )
    set(GAME_EQUIPMENT_ICON_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/cell/asset_equipment_slot_cell.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_weapon_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_offhand_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_head_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_armour_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_hands_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_waist_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_legs_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_feet_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_neck_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_ring_left_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_ring_right_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/slots/asset_slot_icon_relic_empty.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_old_sword.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_padded_jacket.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_leather_greaves.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_iron_sword.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_patched_mail.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_guard_coat.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_iron_greaves.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_militia_axe.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_runner_wraps.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_black_sun_charm.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_miller_hook.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_chain_patches.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_scavenger_knee_plates.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_dragon_ash_token.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/gear/asset_icon_miller_lucky_nail.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/rewards/asset_reward_xp.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/items/asset_icon_seeker_token.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/items/asset_icon_grain_sacks.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/items/asset_icon_contract_progress.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/items/asset_icon_clue_fragment.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/items/asset_icon_burned_chain_bracket.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/equipment_icons_01/items/asset_icon_order_scrap.png"
    )
    add_custom_command(
        OUTPUT "${ASH_BORDER_MAP_PNG}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/generated/world_map_ash_border_01"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_world_map_art.py" --out "${ASH_BORDER_MAP_PNG}"
        DEPENDS tools/generate_world_map_art.py
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating Ash Border world map atlas"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${GAME_PACK_DIR}/game.ntpack" "${CMAKE_CURRENT_SOURCE_DIR}/src/generated/game_assets.h"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_PACK_DIR}"
        COMMAND $<TARGET_FILE:build_game_packs> "${GAME_PACK_DIR}"
        DEPENDS build_game_packs src/build_packs.c
            assets/shaders/slug_text.vert assets/shaders/slug_text.frag
            assets/shaders/mesh_inst.vert assets/shaders/mesh_inst.frag
            assets/shaders/mesh_tex.vert assets/shaders/mesh_tex.frag
            assets/shaders/sprite.vert assets/shaders/sprite.frag assets/shaders/sprite_mask_glow.frag
            assets/ui/panel.png assets/ui/button.png
            assets/ui/tutorial_finger_01.png
            assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_equipment.png
            assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_journal.png
            assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_map.png
            assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_place.png
            assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_more.png
            assets/ui/seeker_portrait_hud.png
            assets/ui/gold_coin_hud.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_portrait_frame.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_status_plaque.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_hp_frame.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_xp_frame.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_resource_coin_chip.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_resource_supplies_chip.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_location_plaque.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_settings_button.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_level_badge.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_icon_coin.png
            assets/ui/generated/top_hud_tokens_02/slices/top_hud_icon_supplies.png
            "${ASH_BORDER_MAP_PNG}"
            ${GAME_COMBAT_ACTOR_FILES}
            ${GAME_LOCATION_SCENE_FILES}
            ${GAME_EQUIPMENT_ICON_FILES}
            assets/ui/slider_track.png assets/ui/slider_fill.png assets/ui/slider_thumb.png
            assets/scenes/last_post_background_candidate05_1280x700.png
            assets/characters/last_post_guard_05.png
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
    src/ui/first_screen_hud.c
    src/ui/bottom_nav.c
    src/ui/location_screen.c
    src/ui/world_map_screen.c
    src/ui/world_map_viewport.c
    src/ui/combat_flow.c
    src/ui/equipment_screen.c
    src/ui/shop_screen.c
    src/ui/game_modal.c
    src/ui/tutorial_callout.c
    src/ui/dialogue_panel.c
    src/ui/ui_runtime.c
    src/ui/theme.c
    src/game_dialogue.c
    src/game_combat.c
    src/game_audio.c
    "${GAME_CONTENT_GENERATED_SOURCE}"
    src/scene/scene_layout.c
    src/scene/scene_interactions.c
    src/render/render_2d_runtime.c
    src/render/render_hub_scene.c
    src/render/render_mesh.c
    src/render/capture.c
    src/systems/sys_move.c
    src/systems/sys_scene_interactions.c
    src/systems/sys_scene_pan.c
    src/systems/sys_settings.c
)
add_dependencies(${GAME_TARGET} content_compatibility_check)
if(NOT EMSCRIPTEN)
    add_dependencies(${GAME_TARGET} game_asset_packs)
    add_dependencies(${GAME_TARGET} game_audio_assets)
endif()
if(FEATURE_GAME_STATE)
    set(GAME_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/game_state.schema.json")
    set(GAME_STATE_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/generate_state.py")
    set(GAME_STATE_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/scripts/game_state.c.in")
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
            "${GAME_STATE_TEMPLATE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        COMMENT "Generating installed game-state feature sources"
        VERBATIM
    )
    target_sources(${GAME_TARGET} PRIVATE
        "${GAME_STATE_GENERATED_SOURCE}"
        src/game_storage.c
        src/game_actions.c
    )
    target_link_libraries(${GAME_TARGET} PRIVATE cjson)
    target_compile_definitions(${GAME_TARGET} PRIVATE FEATURE_GAME_STATE=1)
    target_include_directories(${GAME_TARGET} PRIVATE "${GAME_STATE_GENERATED_DIR}")
    if(GAME_DEVAPI_ENABLED)
        target_sources(${GAME_TARGET} PRIVATE "${GAME_STATE_GENERATED_DEVAPI}")
    endif()
else()
    target_compile_definitions(${GAME_TARGET} PRIVATE FEATURE_GAME_STATE=0)
endif()
target_include_directories(${GAME_TARGET} PRIVATE src "${GAME_SOURCE_GENERATED_DIR}" "${ENGINE_DIR}/deps/glfw/deps")
target_link_libraries(${GAME_TARGET} PRIVATE
    nt_core nt_app nt_input nt_window nt_gfx nt_text_renderer nt_font
    nt_resource nt_material nt_shared nt_render nt_hash nt_fs nt_log nt_math nt_time
    nt_mesh_renderer nt_entity nt_transform_comp nt_mesh_comp nt_material_comp nt_drawable_comp
    nt_ui nt_sprite_renderer nt_atlas nt_mem_scratch nt_clipboard
    glad stb_image_write
)
if(EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http nt_platform_web)
    target_link_options(${GAME_TARGET} PRIVATE
        "SHELL:-sFORCE_FILESYSTEM=1"
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/audio@/assets/audio"
    )
    target_compile_definitions(${GAME_TARGET} PRIVATE GAME_AUDIO_ASSET_DIR="/assets/audio")
else()
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http_stub)
    target_compile_definitions(${GAME_TARGET} PRIVATE GAME_AUDIO_ASSET_DIR="${GAME_AUDIO_ASSETS_DIR}")
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

add_executable(test_scene_layout
    src/scene/scene_layout.c
    tests/test_scene_layout.c
)
target_include_directories(test_scene_layout PRIVATE src)
nt_set_warning_flags(test_scene_layout)
set_target_properties(test_scene_layout PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}/tests")
add_custom_target(scene_layout_test
    COMMAND $<TARGET_FILE:test_scene_layout>
    DEPENDS test_scene_layout
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Running first-scene layout tests"
    VERBATIM
)

if(FEATURE_GAME_STATE)
    add_executable(test_scene_interactions
        src/scene/scene_interactions.c
        src/game_actions.c
        src/game_audio.c
        src/game_combat.c
        src/game_dialogue.c
        "${GAME_CONTENT_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_SOURCE}"
        tests/test_scene_interactions.c
    )
    target_include_directories(test_scene_interactions PRIVATE src)
    target_include_directories(test_scene_interactions PRIVATE "${ENGINE_DIR}/engine")
    target_include_directories(test_scene_interactions PRIVATE "${GAME_STATE_GENERATED_DIR}")
    target_link_libraries(test_scene_interactions PRIVATE cjson)
    target_compile_definitions(test_scene_interactions PRIVATE FEATURE_GAME_STATE=1 NT_DEVAPI_ENABLED=0 NT_PLATFORM_WEB=1 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_scene_interactions)
    set_target_properties(test_scene_interactions PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}/tests")
    add_custom_target(scene_interactions_test
        COMMAND $<TARGET_FILE:test_scene_interactions>
        DEPENDS test_scene_interactions
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        COMMENT "Running scene interaction tests"
        VERBATIM
    )
else()
    add_custom_target(scene_interactions_test
        COMMAND ${CMAKE_COMMAND} -E echo "Skipping scene interaction tests: FEATURE_GAME_STATE=OFF"
        COMMENT "Skipping scene interaction tests"
        VERBATIM
    )
endif()

add_executable(test_game_audio
    src/game_audio.c
    tests/test_game_audio.c
)
target_include_directories(test_game_audio PRIVATE src)
target_compile_definitions(test_game_audio PRIVATE NT_PLATFORM_WEB=1)
target_compile_definitions(test_game_audio PRIVATE GAME_AUDIO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/audio")
nt_set_warning_flags(test_game_audio)
set_target_properties(test_game_audio PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}/tests")
add_custom_target(game_audio_test
    COMMAND $<TARGET_FILE:test_game_audio>
    DEPENDS test_game_audio
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Running game audio tests"
    VERBATIM
)

add_executable(test_world_map_viewport
    src/ui/world_map_viewport.c
    tests/test_world_map_viewport.c
)
target_include_directories(test_world_map_viewport PRIVATE src)
nt_set_warning_flags(test_world_map_viewport)
set_target_properties(test_world_map_viewport PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}/tests")
add_custom_target(world_map_viewport_test
    COMMAND $<TARGET_FILE:test_world_map_viewport>
    DEPENDS test_world_map_viewport
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Running world-map viewport tests"
    VERBATIM
)

if(FEATURE_GAME_STATE)
    add_executable(test_game_dialogue
        src/game_actions.c
        src/game_audio.c
        src/game_combat.c
        src/game_dialogue.c
        "${GAME_CONTENT_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_SOURCE}"
        tests/test_game_dialogue.c
    )
    target_include_directories(test_game_dialogue PRIVATE src)
    target_include_directories(test_game_dialogue PRIVATE "${ENGINE_DIR}/engine")
    target_include_directories(test_game_dialogue PRIVATE "${GAME_STATE_GENERATED_DIR}")
    target_link_libraries(test_game_dialogue PRIVATE cjson)
    target_compile_definitions(test_game_dialogue PRIVATE FEATURE_GAME_STATE=1 NT_DEVAPI_ENABLED=0 NT_PLATFORM_WEB=1 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_game_dialogue)
    set_target_properties(test_game_dialogue PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}/tests")
add_custom_target(game_dialogue_test
        COMMAND $<TARGET_FILE:test_game_dialogue>
        DEPENDS test_game_dialogue
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        COMMENT "Running dialogue state tests"
        VERBATIM
    )

    add_executable(test_game_combat
        src/game_actions.c
        src/game_audio.c
        src/game_combat.c
        src/game_dialogue.c
        "${GAME_CONTENT_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_SOURCE}"
        tests/test_game_combat.c
    )
    target_include_directories(test_game_combat PRIVATE src)
    target_include_directories(test_game_combat PRIVATE "${ENGINE_DIR}/engine")
    target_include_directories(test_game_combat PRIVATE "${GAME_STATE_GENERATED_DIR}")
    target_link_libraries(test_game_combat PRIVATE cjson)
    target_compile_definitions(test_game_combat PRIVATE FEATURE_GAME_STATE=1 NT_DEVAPI_ENABLED=0 NT_PLATFORM_WEB=1 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_game_combat)
    set_target_properties(test_game_combat PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}/tests")
    add_custom_target(game_combat_test
        COMMAND $<TARGET_FILE:test_game_combat>
        DEPENDS test_game_combat
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        COMMENT "Running combat state tests"
        VERBATIM
    )
    add_custom_target(game_state_rules_tests
        DEPENDS game_dialogue_test game_combat_test
    )

    add_executable(test_world_story
        src/game_actions.c
        src/game_combat.c
        "${GAME_CONTENT_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_SOURCE}"
        tests/test_world_story.c
    )
    target_include_directories(test_world_story PRIVATE src)
    target_include_directories(test_world_story PRIVATE "${ENGINE_DIR}/engine")
    target_include_directories(test_world_story PRIVATE "${GAME_STATE_GENERATED_DIR}")
    target_link_libraries(test_world_story PRIVATE cjson)
    target_compile_definitions(test_world_story PRIVATE FEATURE_GAME_STATE=1 NT_DEVAPI_ENABLED=0 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_world_story)
    set_target_properties(test_world_story PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}/tests")
    add_custom_target(world_story_test
        COMMAND $<TARGET_FILE:test_world_story>
        DEPENDS test_world_story
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        COMMENT "Running world story tests"
        VERBATIM
    )
else()
    add_custom_target(game_dialogue_test
        COMMAND ${CMAKE_COMMAND} -E echo "Skipping dialogue state tests: FEATURE_GAME_STATE=OFF"
        COMMENT "Skipping dialogue state tests"
        VERBATIM
    )
    add_custom_target(game_combat_test
        COMMAND ${CMAKE_COMMAND} -E echo "Skipping combat state tests: FEATURE_GAME_STATE=OFF"
        COMMENT "Skipping combat state tests"
        VERBATIM
    )
    add_custom_target(game_state_rules_tests
        DEPENDS game_dialogue_test game_combat_test
    )
    add_custom_target(world_story_test
        COMMAND ${CMAKE_COMMAND} -E echo "Skipping world story tests: FEATURE_GAME_STATE=OFF"
        COMMENT "Skipping world story tests"
        VERBATIM
    )
endif()

add_custom_target(first_scene_tests
    DEPENDS scene_layout_test scene_interactions_test game_state_rules_tests game_audio_test world_map_viewport_test
)

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
            DEPENDS ${GAME_TARGET} first_scene_tests
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
            COMMENT "Capturing QCLR_002 responsive viewport evidence"
            VERBATIM
        )
    endif()
endif()

# --- asset pack builder (runs at build time -> game.ntpack + asset-id header) ---
if(NOT EMSCRIPTEN)
    file(GLOB DRESS_ASSET_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/dress/*.png")
    file(GLOB UI_ASSET_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/*.png")
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
            assets/icons/gold.png assets/icons/xp.png assets/icons/energy.png
            assets/icons/potion.png assets/icons/sword.png assets/icons/wood.png
            assets/audio/sfx/ui_click.wav assets/audio/sfx/awakening_jingle.mp3
            ${DRESS_ASSET_SOURCES} ${UI_ASSET_SOURCES}
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

# Every source asset is a conservative pack dependency. The builder remains
# the content owner; this list only guarantees that an asset edit invalidates
# game.ntpack instead of shipping stale bytes.
file(GLOB_RECURSE GAME_PACK_SOURCE_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.glb"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.mp3"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.wav")

# --- asset pack builder (runs at build time -> game.ntpack + asset-id header) ---
if(NOT EMSCRIPTEN)
    set(GAME_FONT_SOURCE
        "${GAME_REPO_ROOT}/external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf")
    # The generated charset header is a SOURCE, not just a dependency of the
    # pack command below: that is what orders the loc codegen ahead of
    # compiling build_packs.c, which includes it.
    add_executable(build_game_packs src/build_packs.c
        "${GAME_SOURCE_GENERATED_DIR}/loc_charset.gen.h")
    target_include_directories(build_game_packs PRIVATE src)
    target_link_libraries(build_game_packs PRIVATE nt_builder nt_log)
    target_compile_definitions(build_game_packs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(build_game_packs PRIVATE -U_DLL)
    nt_set_sanitizer_flags(build_game_packs)
    set_target_properties(build_game_packs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}")

    add_custom_command(
        OUTPUT "${GAME_PACK_DIR}/game.ntpack" "${CMAKE_CURRENT_SOURCE_DIR}/src/generated/game_assets.h"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_PACK_DIR}"
        COMMAND $<TARGET_FILE:build_game_packs>
            "${GAME_PACK_DIR}" "${GAME_FONT_SOURCE}"
        DEPENDS build_game_packs src/build_packs.c
            "${GAME_FONT_SOURCE}" ${GAME_PACK_SOURCE_ASSETS}
            assets/audio/sfx/ui_click.wav assets/audio/music/demo_jingle.mp3
            # The font atlas is subset to the corpus, so a string edit must
            # invalidate the pack. Without this line a new character regenerates
            # the header, the atlas is NOT repacked, and the glyph ships as a
            # blank box with every gate green.
            "${GAME_SOURCE_GENERATED_DIR}/loc_charset.gen.h"
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

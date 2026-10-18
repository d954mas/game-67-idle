if(NOT EMSCRIPTEN)
    add_executable(build_game_packs src/build_packs.c)
    target_link_libraries(build_game_packs PRIVATE nt_builder nt_log nt_meshwire)
    set_target_properties(build_game_packs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}")
    add_custom_command(
        OUTPUT "${GAME_PACK_DIR}/game.ntpack"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_PACK_DIR}"
        COMMAND $<TARGET_FILE:build_game_packs> "${GAME_PACK_DIR}"
        DEPENDS build_game_packs src/build_packs.c
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${GAME_ASSETS_DIR}/game.ntpack"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_ASSETS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GAME_PACK_DIR}/game.ntpack" "${GAME_ASSETS_DIR}/game.ntpack"
        DEPENDS "${GAME_PACK_DIR}/game.ntpack"
        VERBATIM)
    add_custom_target(game_asset_packs DEPENDS "${GAME_ASSETS_DIR}/game.ntpack")
endif()

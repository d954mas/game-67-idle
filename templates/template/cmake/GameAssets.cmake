file(GLOB_RECURSE GAME_MUSIC_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/music/*.mp3")
set(GAME_MUSIC_PACK_ENABLED 0)
if(GAME_MUSIC_FILES)
    set(GAME_MUSIC_PACK_ENABLED 1)
endif()
target_compile_definitions(${GAME_TARGET} PRIVATE GAME_MUSIC_PACK_ENABLED=${GAME_MUSIC_PACK_ENABLED})
set(GAME_SOURCE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/game")
if(EMSCRIPTEN AND GAME_HOST_GENERATED_DIR)
    set(GAME_SOURCE_GENERATED_DIR "${GAME_HOST_GENERATED_DIR}")
endif()
target_include_directories(${GAME_TARGET} PRIVATE "${GAME_SOURCE_GENERATED_DIR}")
if(GAME_MUSIC_PACK_ENABLED)
    target_sources(${GAME_TARGET} PRIVATE src/game_asset_paths.c)
    target_link_libraries(${GAME_TARGET} PRIVATE nt_resource)
    if(EMSCRIPTEN)
        target_link_libraries(${GAME_TARGET} PRIVATE nt_http)
    else()
        target_link_libraries(${GAME_TARGET} PRIVATE nt_fs nt_http_stub)
    endif()
else()
    # Removing the last track must not leave music in a later export.
    file(REMOVE "${GAME_ASSETS_DIR}/music.ntpack" "${GAME_PACK_DIR}/music.ntpack"
        "${GAME_SOURCE_GENERATED_DIR}/music.h")
endif()

if(NOT EMSCRIPTEN)
    set(GAME_MUSIC_OUTPUTS)
    set(GAME_MUSIC_STAGED)
    if(GAME_MUSIC_PACK_ENABLED)
        set(GAME_MUSIC_OUTPUTS "${GAME_PACK_DIR}/music.ntpack" "${GAME_SOURCE_GENERATED_DIR}/music.h")
        set(GAME_MUSIC_STAGED "${GAME_ASSETS_DIR}/music.ntpack")
    endif()
    add_executable(build_game_packs src/build_packs.c)
    target_compile_definitions(build_game_packs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(build_game_packs PRIVATE nt_builder nt_log nt_meshwire)
    set_target_properties(build_game_packs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}")
    add_custom_command(
        OUTPUT "${GAME_PACK_DIR}/game.ntpack" "${GAME_SOURCE_GENERATED_DIR}/game.h" ${GAME_MUSIC_OUTPUTS}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_PACK_DIR}" "${GAME_SOURCE_GENERATED_DIR}"
        COMMAND $<TARGET_FILE:build_game_packs> "${GAME_PACK_DIR}" "${GAME_SOURCE_GENERATED_DIR}" ${GAME_MUSIC_FILES}
        DEPENDS build_game_packs src/build_packs.c ${GAME_MUSIC_FILES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${GAME_ASSETS_DIR}/game.ntpack"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_ASSETS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GAME_PACK_DIR}/game.ntpack" "${GAME_ASSETS_DIR}/game.ntpack"
        DEPENDS "${GAME_PACK_DIR}/game.ntpack"
        VERBATIM)
    if(GAME_MUSIC_PACK_ENABLED)
        add_custom_command(OUTPUT "${GAME_ASSETS_DIR}/music.ntpack"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${GAME_PACK_DIR}/music.ntpack" "${GAME_ASSETS_DIR}/music.ntpack"
            DEPENDS "${GAME_PACK_DIR}/music.ntpack" "${GAME_ASSETS_DIR}/game.ntpack"
            VERBATIM)
    endif()
    add_custom_target(game_asset_packs DEPENDS "${GAME_ASSETS_DIR}/game.ntpack" ${GAME_MUSIC_STAGED})
endif()

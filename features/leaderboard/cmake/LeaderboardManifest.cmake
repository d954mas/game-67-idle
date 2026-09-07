# Generates the board table from a game's leaderboards.json.
#
# The header is a build product, not a checked-in file: which backend family and
# which portal board id a build gets is a publish-target answer, so regenerating
# it is how a target switch stays honest.
#
#   leaderboard_generate_boards(
#       TARGET         game
#       MANIFEST       "${CMAKE_CURRENT_SOURCE_DIR}/leaderboards.json"
#       PUBLISH_TARGET "${GAME_PUBLISH_TARGET}"
#       OUTPUT_DIR     "${CMAKE_CURRENT_BINARY_DIR}/generated")
#
# The target then includes "game_leaderboards.h".

function(leaderboard_generate_boards)
    cmake_parse_arguments(LB "" "TARGET;MANIFEST;PUBLISH_TARGET;OUTPUT_DIR" "" ${ARGN})
    foreach(required TARGET MANIFEST PUBLISH_TARGET OUTPUT_DIR)
        if(NOT LB_${required})
            message(FATAL_ERROR "leaderboard_generate_boards: ${required} is required")
        endif()
    endforeach()

    find_program(LEADERBOARD_NODE_EXECUTABLE node REQUIRED)

    set(_lb_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    set(_lb_script "${_lb_dir}/scripts/leaderboards.mjs")
    set(_lb_lib "${_lb_dir}/lib/leaderboards.mjs")
    set(_lb_header "${LB_OUTPUT_DIR}/game_leaderboards.h")

    add_custom_command(
        OUTPUT "${_lb_header}"
        COMMAND "${LEADERBOARD_NODE_EXECUTABLE}" "${_lb_script}" generate
                --manifest "${LB_MANIFEST}"
                --target "${LB_PUBLISH_TARGET}"
                --out "${_lb_header}"
        DEPENDS "${LB_MANIFEST}" "${_lb_script}" "${_lb_lib}"
        COMMENT "leaderboards: board table for ${LB_PUBLISH_TARGET}"
        VERBATIM)

    add_custom_target(${LB_TARGET}_leaderboard_manifest DEPENDS "${_lb_header}")
    add_dependencies(${LB_TARGET} ${LB_TARGET}_leaderboard_manifest)
    target_include_directories(${LB_TARGET} PRIVATE "${LB_OUTPUT_DIR}")
endfunction()

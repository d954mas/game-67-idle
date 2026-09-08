include("${CMAKE_CURRENT_LIST_DIR}/LeaderboardManifest.cmake")

function(leaderboard_add_example_test)
    set(example_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../example")
    set(source_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src")
    game_add_c_test(test_leaderboard_example WARNINGS TIER core
        SOURCES "${example_dir}/test_lap_time_example.c"
                "${example_dir}/lap_time_example.c"
                "${source_dir}/leaderboard.c"
                "${source_dir}/leaderboard_core.c"
                "${source_dir}/leaderboard_extra.c"
                "${source_dir}/leaderboard_mock.c"
        LIBS cjson nt_base64
        DEFINES LEADERBOARD_TESTING=1
        INCLUDES "${example_dir}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../include")
    leaderboard_generate_boards(
        TARGET test_leaderboard_example
        MANIFEST "${example_dir}/leaderboards.json"
        PUBLISH_TARGET local
        OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/leaderboard-example")
endfunction()

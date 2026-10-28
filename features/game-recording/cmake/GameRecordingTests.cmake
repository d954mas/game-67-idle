include_guard(GLOBAL)

function(game_recording_add_tests)
    set(recording_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    game_add_c_test(test_game_recording WARNINGS TIER core
        SOURCES "${recording_dir}/tests/test_game_recording.c"
                "${recording_dir}/src/recording.c"
        LIBS cjson
        DEFINES NT_DEVAPI_ENABLED=1 NT_DEVAPI_GROUP_RECORDER=1 AUDIO_CORE_CAPTURE_ENABLED=1
        INCLUDES "${recording_dir}/include"
                 "${recording_dir}/../audio-core/include"
                 "${GAME_REPO_ROOT}/external/neotolis-engine/engine")
endfunction()

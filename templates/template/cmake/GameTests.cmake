if(NOT EMSCRIPTEN)
    enable_testing()
    function(game_add_c_test name)
        cmake_parse_arguments(T "" "" "SOURCES;LIBS;INCLUDES;DEFINES" ${ARGN})
        add_executable(${name} ${T_SOURCES})
        target_link_libraries(${name} PRIVATE unity ${T_LIBS})
        target_include_directories(${name} PRIVATE ${T_INCLUDES})
        target_compile_definitions(${name} PRIVATE ${T_DEFINES} _CRT_SECURE_NO_WARNINGS)
        nt_set_warning_flags(${name})
        nt_set_sanitizer_flags(${name})
        set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
        add_test(NAME ${name} COMMAND ${name})
        set_tests_properties(${name} PROPERTIES LABELS "core")
    endfunction()
    game_add_c_test(test_game_input
        SOURCES tests/test_game_input.c src/game_input.c
        LIBS nt_input_stub INCLUDES src)
    game_add_c_test(test_game_music_pack
        SOURCES tests/test_game_music_pack.c src/game_music_pack.c
        LIBS nt_hash INCLUDES src
        DEFINES GAME_MUSIC_PACK_ENABLED=1)
    add_test(NAME music_packaging
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_music_packaging.py"
            --builder $<TARGET_FILE:build_game_packs>)
    set_tests_properties(music_packaging PROPERTIES LABELS "core")
    game_add_c_test(test_game_state_roundtrip
        SOURCES tests/test_game_state_roundtrip.c "${GAME_STATE_GENERATED_SOURCE}"
                "${GAME_STATE_SRC}/game_state_json.c"
                "${GAME_STATE_SRC}/game_save_text.c"
                "${GAME_STATE_SRC}/game_save_writer.c"
        LIBS cjson INCLUDES "${GAME_STATE_INC}" "${GAME_STATE_GENERATED_DIR}"
        DEFINES NT_DEVAPI_ENABLED=0)
endif()

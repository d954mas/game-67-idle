# --- native C unit tests (Unity + CTest) ---

# The tier vocabulary is fixed for every game so a runner, a CI lane and a
# reader mean the same thing by it. An unknown tier is a configure error rather
# than a test that silently never runs.
set(GAME_TEST_TIERS core slow taste)

function(game_test_tier_label out tier name)
    if(NOT tier)
        set(tier core)
    endif()
    if(NOT tier IN_LIST GAME_TEST_TIERS)
        message(FATAL_ERROR "${name}: unknown test tier '${tier}', expected one of: ${GAME_TEST_TIERS}")
    endif()
    set(${out} "${tier}" PARENT_SCOPE)
endfunction()

# One native test: an executable in build/tests, registered with CTest under its
# own name. Everything a test cannot choose -- output directory, CRT define,
# sanitizer flags, the add_test line -- lives here; everything it can choose
# stays an argument. A new plain test is three lines, and nothing outside this
# call has to learn about it.
#
# WARNINGS is opt-in because not every test source compiles clean under the
# engine's -W set with -Werror, and turning it on for all of them is a separate
# decision from removing the boilerplate.
#
# TIER is the test's place in the loop, and it is a CTest label so that nothing
# outside the test has to keep a list:
#   core  (default) silent-failure logic, runs on every edit, stays fast;
#   slow            correct but expensive -- heavy fixtures, sims, packaging;
#   taste           pins player-facing output, moves with design, runs before
#                   release.
function(game_add_c_test name)
    cmake_parse_arguments(T "WARNINGS" "UNITY;WORKDIR;TIER"
        "SOURCES;LIBS;INCLUDES;DEFINES;OPTIONS" ${ARGN})
    if(NOT T_UNITY)
        set(T_UNITY unity)
    endif()
    game_test_tier_label(T_TIER "${T_TIER}" "${name}")
    add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE ${T_UNITY} ${T_LIBS})
    if(T_INCLUDES)
        target_include_directories(${name} PRIVATE ${T_INCLUDES})
    endif()
    target_compile_definitions(${name} PRIVATE ${T_DEFINES} _CRT_SECURE_NO_WARNINGS)
    if(T_OPTIONS)
        target_compile_options(${name} PRIVATE ${T_OPTIONS})
    endif()
    if(T_WARNINGS)
        nt_set_warning_flags(${name})
    endif()
    nt_set_sanitizer_flags(${name})
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    if(T_WORKDIR)
        add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY "${T_WORKDIR}")
    else()
        add_test(NAME ${name} COMMAND ${name})
    endif()
    set_tests_properties(${name} PROPERTIES LABELS "${T_TIER}")
endfunction()

# Every plain logic test in tests/plain: one file, no build-system edit. A test
# lands here when it needs nothing but Unity, the game's headers and the golden
# bank; anything that must link a subsystem gets its own game_add_c_test call
# above, where the reader can see what it pulls in.
function(game_add_plain_c_tests)
    file(GLOB _plain_tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/plain/test_*.c")
    foreach(_plain_test IN LISTS _plain_tests)
        get_filename_component(_plain_name "${_plain_test}" NAME_WE)
        game_add_c_test(${_plain_name}
            SOURCES "${_plain_test}" "${TEST_GOLDENS_SRC}/test_goldens.c"
            INCLUDES src "${TEST_GOLDENS_INC}"
            WORKDIR "${CMAKE_BINARY_DIR}/tests"
            WARNINGS)
    endforeach()
endfunction()

# One Node contract test over a script path, run from the studio root so a test
# may reach both the game and the feature it exercises.
function(game_add_node_test name)
    cmake_parse_arguments(N "" "TIER" "SCRIPTS" ${ARGN})
    game_test_tier_label(N_TIER "${N_TIER}" "${name}")
    if(NOT N_SCRIPTS)
        set(N_SCRIPTS ${N_UNPARSED_ARGUMENTS})
    endif()
    find_program(GAME_TEST_NODE_EXECUTABLE node REQUIRED)
    add_test(NAME ${name}
        COMMAND "${GAME_TEST_NODE_EXECUTABLE}" --test ${N_SCRIPTS}
        WORKING_DIRECTORY "${GAME_REPO_ROOT}")
    set_tests_properties(${name} PROPERTIES LABELS "${N_TIER}")
endfunction()

if(NOT EMSCRIPTEN)
    enable_testing()

    set(TEST_GOLDENS_DIR "${GAME_REPO_ROOT}/features/test-goldens")
    set(TEST_GOLDENS_INC "${TEST_GOLDENS_DIR}/include")
    set(TEST_GOLDENS_SRC "${TEST_GOLDENS_DIR}/src")

    game_add_c_test(test_test_goldens
        SOURCES "${TEST_GOLDENS_DIR}/tests/test_test_goldens.c" "${TEST_GOLDENS_SRC}/test_goldens.c"
        INCLUDES "${TEST_GOLDENS_INC}"
        WORKDIR "${CMAKE_BINARY_DIR}/tests"
        WARNINGS)

    game_add_plain_c_tests()
    include_directories("${GAME_STATE_INC}")
    set(SCENES_CORE_TESTS "${SCENES_CORE_DIR}/tests")
    set(AUDIO_CORE_TESTS "${AUDIO_CORE_DIR}/tests")
    add_library(audio_unity STATIC "${ENGINE_DIR}/deps/unity/src/unity.c")
    target_include_directories(audio_unity PUBLIC "${ENGINE_DIR}/deps/unity/src")
    target_compile_definitions(audio_unity PRIVATE _CRT_SECURE_NO_WARNINGS)

    add_executable(test_scenes_core_catalog
        "${SCENES_CORE_TESTS}/test_scene_manager_catalog.c"
        "${SCENES_CORE_SRC}/scene_manager.c")
    target_link_libraries(test_scenes_core_catalog PRIVATE unity)
    target_include_directories(test_scenes_core_catalog PRIVATE "${SCENES_CORE_INC}")
    target_compile_definitions(test_scenes_core_catalog PRIVATE _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_scenes_core_catalog)
    set_target_properties(test_scenes_core_catalog PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_scenes_core_catalog COMMAND test_scenes_core_catalog)
    set_tests_properties(test_scenes_core_catalog PROPERTIES LABELS "core")

    foreach(_scenes_core_source IN ITEMS
            test_scene_manager_lifecycle.c
            test_scene_manager_navigation.c
            test_scene_manager_ordering.c
            test_scene_manager_presentation.c)
        string(REGEX REPLACE "^test_scene_manager_(.+)\\.c$" "\\1"
            _scenes_core_suite "${_scenes_core_source}")
        add_executable(test_scenes_core_${_scenes_core_suite}
            "${SCENES_CORE_TESTS}/${_scenes_core_source}"
            "${SCENES_CORE_SRC}/scene_manager.c")
        target_link_libraries(test_scenes_core_${_scenes_core_suite} PRIVATE unity)
        target_include_directories(test_scenes_core_${_scenes_core_suite}
            PRIVATE "${SCENES_CORE_INC}")
        target_compile_definitions(test_scenes_core_${_scenes_core_suite}
            PRIVATE _CRT_SECURE_NO_WARNINGS)
        nt_set_warning_flags(test_scenes_core_${_scenes_core_suite})
        set_target_properties(test_scenes_core_${_scenes_core_suite} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
        add_test(NAME test_scenes_core_${_scenes_core_suite}
            COMMAND test_scenes_core_${_scenes_core_suite})
        set_tests_properties(test_scenes_core_${_scenes_core_suite} PROPERTIES LABELS "core")
    endforeach()

    add_executable(test_scenes_core_deadlines
        "${SCENES_CORE_TESTS}/test_scene_manager_deadlines.c")
    target_link_libraries(test_scenes_core_deadlines PRIVATE unity)
    target_include_directories(test_scenes_core_deadlines PRIVATE
        "${SCENES_CORE_INC}")
    target_compile_definitions(test_scenes_core_deadlines PRIVATE
        SCENE_MANAGER_LOAD_DEADLINE_STEPS=1
        SCENE_MANAGER_TRANSITION_DEADLINE_STEPS=1
        _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_scenes_core_deadlines)
    set_target_properties(test_scenes_core_deadlines PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_scenes_core_deadlines
        COMMAND test_scenes_core_deadlines)
    set_tests_properties(test_scenes_core_deadlines PROPERTIES LABELS "core")

    add_executable(test_scenes_core_reentrancy
        "${SCENES_CORE_TESTS}/test_scene_manager_reentrancy.c")
    target_link_libraries(test_scenes_core_reentrancy PRIVATE unity)
    target_include_directories(test_scenes_core_reentrancy PRIVATE
        "${SCENES_CORE_INC}")
    target_compile_definitions(test_scenes_core_reentrancy PRIVATE
        _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_scenes_core_reentrancy)
    set_target_properties(test_scenes_core_reentrancy PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_scenes_core_reentrancy
        COMMAND test_scenes_core_reentrancy)
    set_tests_properties(test_scenes_core_reentrancy PROPERTIES LABELS "core")

    if(GAME_DEVAPI_ENABLED)
        add_executable(test_scenes_core_devapi
            "${SCENES_CORE_TESTS}/test_scene_manager_devapi.c"
            "${SCENES_CORE_SRC}/scene_manager.c"
            "${SCENES_CORE_SRC}/scene_manager_devapi.c")
        target_link_libraries(test_scenes_core_devapi PRIVATE
            unity cjson nt_devapi_default nt_app_stub)
        target_include_directories(test_scenes_core_devapi PRIVATE
            "${SCENES_CORE_INC}")
        target_compile_definitions(test_scenes_core_devapi PRIVATE
            NT_DEVAPI_ENABLED=1 _CRT_SECURE_NO_WARNINGS)
        nt_set_warning_flags(test_scenes_core_devapi)
        set_target_properties(test_scenes_core_devapi PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
        add_test(NAME test_scenes_core_devapi COMMAND test_scenes_core_devapi)
        set_tests_properties(test_scenes_core_devapi PROPERTIES LABELS "core")
    endif()

    add_executable(test_scenes_core_devapi_disabled
        "${SCENES_CORE_TESTS}/test_scene_manager_devapi_disabled.c"
        "${SCENES_CORE_SRC}/scene_manager_devapi.c")
    target_include_directories(test_scenes_core_devapi_disabled PRIVATE
        "${SCENES_CORE_INC}" "${SCENES_CORE_SRC}")
    target_compile_definitions(test_scenes_core_devapi_disabled PRIVATE
        NT_DEVAPI_ENABLED=0 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_scenes_core_devapi_disabled)
    set_target_properties(test_scenes_core_devapi_disabled PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_scenes_core_devapi_disabled
        COMMAND test_scenes_core_devapi_disabled)
    set_tests_properties(test_scenes_core_devapi_disabled PROPERTIES LABELS "core")

    add_executable(test_audio_core
        "${AUDIO_CORE_TESTS}/test_audio.c"
        "${AUDIO_CORE_TESTS}/fake_audio_environment.c"
        "${AUDIO_CORE_SRC}/audio.c")
    target_link_libraries(test_audio_core PRIVATE audio_unity nt_core)
    target_include_directories(test_audio_core PRIVATE
        "${AUDIO_CORE_INC}" "${AUDIO_CORE_SRC}" "${AUDIO_CORE_TESTS}")
    target_compile_definitions(test_audio_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(test_audio_core PRIVATE -UUNITY_EXCLUDE_FLOAT -UUNITY_EXCLUDE_DOUBLE)
    nt_set_warning_flags(test_audio_core)
    set_target_properties(test_audio_core PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_audio_core COMMAND test_audio_core)
    set_tests_properties(test_audio_core PROPERTIES LABELS "core")

    add_executable(test_audio_resource
        "${AUDIO_CORE_TESTS}/test_audio_resource.c"
        "${AUDIO_CORE_SRC}/audio_resource.c")
    target_link_libraries(test_audio_resource PRIVATE unity nt_shared)
    target_include_directories(test_audio_resource PRIVATE "${AUDIO_CORE_SRC}" "${ENGINE_DIR}/engine")
    target_compile_definitions(test_audio_resource PRIVATE NT_INTROSPECT_ENABLED=0 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_audio_resource)
    set_target_properties(test_audio_resource PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_audio_resource COMMAND test_audio_resource)
    set_tests_properties(test_audio_resource PROPERTIES LABELS "core")

    set(TEMPLATE_AUDIO_CUE_MP3 "${CMAKE_BINARY_DIR}/tests/fixtures/ui_click.mp3")
    add_executable(write_audio_test_fixture
        tests/fixtures/audio/write_ui_click_mp3.c)
    target_compile_definitions(write_audio_test_fixture PRIVATE _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(write_audio_test_fixture)
    add_custom_command(
        OUTPUT "${TEMPLATE_AUDIO_CUE_MP3}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/tests/fixtures"
        COMMAND $<TARGET_FILE:write_audio_test_fixture> "${TEMPLATE_AUDIO_CUE_MP3}"
        DEPENDS write_audio_test_fixture tests/fixtures/audio/write_ui_click_mp3.c
        VERBATIM)
    add_custom_target(template_audio_test_fixture DEPENDS "${TEMPLATE_AUDIO_CUE_MP3}")

    add_executable(test_audio_backend_native
        "${AUDIO_CORE_TESTS}/test_audio_backend_native.c"
        "${AUDIO_CORE_SRC}/audio_backend_miniaudio.c"
        "${AUDIO_CORE_SRC}/audio_miniaudio_impl.c")
    target_link_libraries(test_audio_backend_native PRIVATE unity)
    target_include_directories(test_audio_backend_native PRIVATE "${AUDIO_CORE_SRC}" "${AUDIO_CORE_VENDOR}")
    target_compile_definitions(test_audio_backend_native PRIVATE
        AUDIO_MINIAUDIO_TEST_NO_DEVICE=1
        AUDIO_TEST_MP3_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/music/demo_jingle.mp3"
        AUDIO_TEST_CUE_WAV_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/sfx/ui_click.wav"
        AUDIO_TEST_CUE_MP3_PATH="${TEMPLATE_AUDIO_CUE_MP3}"
        _CRT_SECURE_NO_WARNINGS)
    add_dependencies(test_audio_backend_native template_audio_test_fixture)
    audio_core_link_native_systems(test_audio_backend_native)
    nt_set_warning_flags(test_audio_backend_native)
    set_target_properties(test_audio_backend_native PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_audio_backend_native COMMAND test_audio_backend_native)
    set_tests_properties(test_audio_backend_native PROPERTIES LABELS "core")

    add_executable(test_game_audio tests/test_game_audio.c src/game_audio.c)
    target_link_libraries(test_game_audio PRIVATE audio_unity nt_ui_interface nt_shared nt_log)
    target_include_directories(test_game_audio PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/audio"
        "${AUDIO_CORE_INC}" "${PLATFORM_SDK_INC}" src "${ENGINE_DIR}/engine")
    target_compile_definitions(test_game_audio PRIVATE NT_INTROSPECT_ENABLED=0 _CRT_SECURE_NO_WARNINGS)
    target_compile_options(test_game_audio PRIVATE -UUNITY_EXCLUDE_FLOAT -UUNITY_EXCLUDE_DOUBLE)
    nt_set_warning_flags(test_game_audio)
    set_target_properties(test_game_audio PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_audio COMMAND test_game_audio)
    set_tests_properties(test_game_audio PROPERTIES LABELS "core")

    find_program(AUDIO_NODE_EXECUTABLE node REQUIRED)
    add_test(NAME test_audio_web_library
        COMMAND "${AUDIO_NODE_EXECUTABLE}" --test "${AUDIO_CORE_TESTS}/test_audio_web_library.mjs"
        WORKING_DIRECTORY "${GAME_REPO_ROOT}")
    set_tests_properties(test_audio_web_library PROPERTIES LABELS "core")

    add_executable(test_game_state_json tests/test_game_state_json.c "${GAME_STATE_SRC}/game_state_json.c")
    target_link_libraries(test_game_state_json PRIVATE cjson unity)
    target_include_directories(test_game_state_json PRIVATE src)
    target_compile_definitions(test_game_state_json PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_state_json PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_state_json COMMAND test_game_state_json)
    set_tests_properties(test_game_state_json PROPERTIES LABELS "core")

    set(NESTED_STATE_TEST_SCHEMA
        "${GAME_REPO_ROOT}/features/game-state/tests/items_containers.schema.json")
    set(NESTED_STATE_TEST_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/game-state-nested-test")
    set(NESTED_STATE_TEST_SOURCE "${NESTED_STATE_TEST_GENERATED_DIR}/items_v2_state.c")
    add_custom_command(
        OUTPUT
            "${NESTED_STATE_TEST_GENERATED_DIR}/items_v2_state.h"
            "${NESTED_STATE_TEST_SOURCE}"
            "${NESTED_STATE_TEST_GENERATED_DIR}/items_v2_state_schema.gen.h"
            "${NESTED_STATE_TEST_GENERATED_DIR}/items_v2_state_events.gen.h"
            "${NESTED_STATE_TEST_GENERATED_DIR}/items_v2_state_events.gen.c"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${NESTED_STATE_TEST_GENERATED_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_GENERATOR}"
            --schema "${NESTED_STATE_TEST_SCHEMA}"
            --out-dir "${NESTED_STATE_TEST_GENERATED_DIR}"
            --fragment items_v2
        DEPENDS "${NESTED_STATE_TEST_SCHEMA}" ${GAME_STATE_GENERATOR_SOURCES}
        WORKING_DIRECTORY "${GAME_REPO_ROOT}"
        COMMENT "Generating nested game-state test fixture"
        VERBATIM)
    add_executable(test_game_state_nested
        tests/test_game_state_nested.c
        "${GAME_STATE_SRC}/game_state_json.c"
        "${NESTED_STATE_TEST_SOURCE}")
    target_link_libraries(test_game_state_nested PRIVATE cjson unity)
    target_include_directories(test_game_state_nested PRIVATE
        src "${NESTED_STATE_TEST_GENERATED_DIR}")
    target_compile_definitions(test_game_state_nested PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_state_nested PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_state_nested COMMAND test_game_state_nested)
    set_tests_properties(test_game_state_nested PROPERTIES LABELS "core")

    # Feature-owned suites. A feature ships its tests, but only a consumer can
    # build them, and the template is the reference consumer -- so they run here
    # instead of living in whichever game happened to wire them up.
    set(SCALAR_STATE_TEST_SCHEMA "${GAME_STATE_DIR}/tests/scalar_state.schema.json")
    set(SCALAR_STATE_TEST_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/game-state-scalar-test")
    set(SCALAR_STATE_TEST_SOURCE "${SCALAR_STATE_TEST_GENERATED_DIR}/scalar_state.c")
    add_custom_command(
        OUTPUT
            "${SCALAR_STATE_TEST_GENERATED_DIR}/scalar_state.h"
            "${SCALAR_STATE_TEST_SOURCE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SCALAR_STATE_TEST_GENERATED_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_GENERATOR}"
            --schema "${SCALAR_STATE_TEST_SCHEMA}"
            --out-dir "${SCALAR_STATE_TEST_GENERATED_DIR}"
            --fragment scalar
        DEPENDS "${SCALAR_STATE_TEST_SCHEMA}" ${GAME_STATE_GENERATOR_SOURCES}
        WORKING_DIRECTORY "${GAME_REPO_ROOT}"
        COMMENT "Generating scalar game-state test fixture"
        VERBATIM)

    game_add_c_test(test_game_save_text
        SOURCES "${GAME_STATE_DIR}/tests/test_game_save_text.c" "${GAME_STATE_SRC}/game_save_text.c"
        INCLUDES "${GAME_STATE_INC}" "${GAME_STATE_SRC}")

    game_add_c_test(test_game_save_writer
        SOURCES "${GAME_STATE_DIR}/tests/test_game_save_writer.c"
                "${GAME_STATE_SRC}/game_save_writer.c" "${GAME_STATE_SRC}/game_save_text.c"
        LIBS cjson
        INCLUDES "${GAME_STATE_INC}" "${GAME_STATE_SRC}")

    game_add_c_test(test_generated_snapshot_over512
        SOURCES "${GAME_STATE_DIR}/tests/test_generated_snapshot_over512.c"
                "${GAME_STATE_SRC}/game_save_writer.c" "${GAME_STATE_SRC}/game_save_text.c"
                "${GAME_STATE_SRC}/game_state_json.c" "${NESTED_STATE_TEST_SOURCE}"
        LIBS cjson
        INCLUDES "${GAME_STATE_INC}" "${GAME_STATE_SRC}" "${NESTED_STATE_TEST_GENERATED_DIR}")

    game_add_c_test(test_generated_text_codec
        SOURCES "${GAME_STATE_DIR}/tests/test_generated_text_codec.c"
                "${GAME_STATE_SRC}/game_save_text.c" "${GAME_STATE_SRC}/game_state_json.c"
                "${GAME_STATE_SRC}/game_save_writer.c" "${SCALAR_STATE_TEST_SOURCE}"
        LIBS cjson
        UNITY audio_unity
        OPTIONS -UUNITY_EXCLUDE_FLOAT -UUNITY_EXCLUDE_DOUBLE
        INCLUDES "${GAME_STATE_INC}" "${GAME_STATE_SRC}" "${SCALAR_STATE_TEST_GENERATED_DIR}")

    set(SOFTWARE_CURSOR_DIR "${GAME_REPO_ROOT}/features/software-cursor")
    game_add_c_test(test_software_cursor
        SOURCES "${SOFTWARE_CURSOR_DIR}/tests/test_software_cursor.c"
                "${SOFTWARE_CURSOR_DIR}/src/software_cursor.c"
                "${SOFTWARE_CURSOR_DIR}/src/software_cursor_samples.c"
        INCLUDES "${SOFTWARE_CURSOR_DIR}/include"
        UNITY audio_unity
        OPTIONS -UUNITY_EXCLUDE_FLOAT -UUNITY_EXCLUDE_DOUBLE)

    add_executable(test_game_storage
        tests/test_game_storage.c
        "${GAME_STATE_SRC}/game_storage.c"
        "${GAME_STATE_SRC}/game_storage_backend_native.c")
    # Native backend warns via nt_log_warn on a read ERROR.
    target_link_libraries(test_game_storage PRIVATE unity nt_log nt_core nt_hash)
    target_include_directories(test_game_storage PRIVATE src)
    target_compile_definitions(test_game_storage PRIVATE
        GAME_STORAGE_APP_ID="template_storage_test"
        GAME_STORAGE_NATIVE_ROOT="${CMAKE_BINARY_DIR}/tests/build/storage"
        _CRT_SECURE_NO_WARNINGS)
    # MoveFileExA (native quarantine/atomic-replace) is in kernel32, linked by default.
    set_target_properties(test_game_storage PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_storage COMMAND test_game_storage
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    set_tests_properties(test_game_storage PROPERTIES LABELS "core")

    # T0055: the contract between the two write modes lives beside the FEATURE,
    # not in a game's test file -- it is game_storage being tested, not the game.
    # Its own storage root: these tests deliberately hold handles on their slots,
    # and nothing else should be writing into the same directory while they do.
    add_executable(test_game_storage_write_modes
        "${GAME_STATE_DIR}/tests/test_game_storage_write_modes.c"
        "${GAME_STATE_SRC}/game_storage.c"
        "${GAME_STATE_SRC}/game_storage_backend_native.c")
    target_link_libraries(test_game_storage_write_modes PRIVATE
        unity nt_log nt_core nt_hash)
    target_include_directories(test_game_storage_write_modes PRIVATE
        "${GAME_STATE_INC}" "${GAME_STATE_SRC}")
    target_compile_definitions(test_game_storage_write_modes PRIVATE
        GAME_STORAGE_APP_ID="storage_write_modes_test"
        GAME_STORAGE_NATIVE_ROOT="${CMAKE_BINARY_DIR}/tests/build/write_modes"
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_storage_write_modes PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_storage_write_modes
        COMMAND test_game_storage_write_modes)
    set_tests_properties(test_game_storage_write_modes PROPERTIES LABELS "core")

    add_executable(test_game_storage_web_backend
        "${GAME_STATE_DIR}/tests/test_game_storage_backend_web.c"
        "${GAME_STATE_SRC}/game_storage.c"
        "${GAME_STATE_SRC}/game_storage_backend_web.c")
    target_link_libraries(test_game_storage_web_backend PRIVATE
        unity nt_log nt_core nt_hash)
    target_include_directories(test_game_storage_web_backend PRIVATE
        "${GAME_STATE_INC}" "${GAME_STATE_SRC}")
    target_compile_definitions(test_game_storage_web_backend PRIVATE
        GAME_STORAGE_APP_ID="web_storage_test" _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_storage_web_backend PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_storage_web_backend
        COMMAND test_game_storage_web_backend)
    set_tests_properties(test_game_storage_web_backend PROPERTIES LABELS "core")

    add_executable(test_game_save_blocked
        "${GAME_STATE_DIR}/tests/test_game_save_blocked.c"
        "${GAME_STATE_SRC}/game_save.c"
        "${GAME_STATE_SRC}/game_save_writer.c"
        "${GAME_STATE_SRC}/game_state_json.c")
    target_link_libraries(test_game_save_blocked PRIVATE
        cjson unity nt_log nt_core nt_hash)
    target_include_directories(test_game_save_blocked PRIVATE
        "${GAME_STATE_INC}" "${GAME_STATE_SRC}")
    target_compile_definitions(test_game_save_blocked PRIVATE
        GAME_SAVE_TESTING=1
        GAME_STORAGE_APP_ID="blocked_save_test"
        GAME_SAVE_AUTOSAVE_SLOT="test_slot"
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_save_blocked PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_save_blocked COMMAND test_game_save_blocked)
    set_tests_properties(test_game_save_blocked PROPERTIES LABELS "core")

    add_executable(test_game_save
        tests/test_game_save.c
        "${GAME_STATE_SRC}/game_save.c" "${GAME_STATE_SRC}/game_save_writer.c" "${GAME_STATE_SRC}/game_storage.c"
        "${GAME_STATE_SRC}/game_storage_backend_native.c"
        "${GAME_STATE_SRC}/game_state_json.c")
    # game_save/native storage backend warn via nt_log_warn on read errors.
    target_link_libraries(test_game_save PRIVATE cjson unity nt_log nt_core nt_hash)
    target_include_directories(test_game_save PRIVATE src)
    target_compile_definitions(test_game_save PRIVATE
        GAME_SAVE_TESTING=1
        GAME_STORAGE_APP_ID="template_save_test"
        GAME_STORAGE_NATIVE_ROOT="${CMAKE_BINARY_DIR}/tests/build/save"
        GAME_SAVE_AUTOSAVE_SLOT="test_slot"
        GAME_SAVE_DEBOUNCE_MS=2000
        GAME_SAVE_MAX_INTERVAL_MS=30000
        GAME_SAVE_DOC_VERSION=2
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_save PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_save COMMAND test_game_save
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    set_tests_properties(test_game_save PROPERTIES LABELS "core")

    # (1) основной: позитив + death-тесты переполнения/фазы (NT_ASSERT ФИРИТ)
    add_executable(test_game_events tests/test_game_events.c "${GAME_EVENTS_SRC}/game_events.c"
        "${ENGINE_DIR}/tests/unit/test_helpers/nt_assert_trap.c")
    target_link_libraries(test_game_events PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events PRIVATE "${GAME_EVENTS_INC}" src "${ENGINE_DIR}/tests/unit")
    target_compile_definitions(test_game_events PRIVATE
        GAME_EVENTS_ARENA_BYTES=1024u   # маленькая арена -> переполнение дёшево (позитив влезает)
        GAME_EVENTS_LOG_CAP=64          # маленький кап лога -> переполнение дёшево
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events COMMAND test_game_events)
    set_tests_properties(test_game_events PROPERTIES LABELS "core")

    # (2) overflow-drop: тот же файл + GAME_EVENTS_SOFT_OVERFLOW=1 -> emit ДРОПАЕТ
    # (не assert'ит) в debug-ctest, проверяет release-семантику (тест #10). Не
    # звено с nt_assert_trap: этот бинарь не использует NT_TEST_EXPECT_ASSERT
    # (test_game_events.c гейтит его #include под #ifndef GAME_EVENTS_SOFT_OVERFLOW).
    add_executable(test_game_events_overflow tests/test_game_events.c "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_events_overflow PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events_overflow PRIVATE "${GAME_EVENTS_INC}" src)
    target_compile_definitions(test_game_events_overflow PRIVATE
        GAME_EVENTS_SOFT_OVERFLOW=1     # выключить debug-assert переполнения -> тестировать дроп
        GAME_EVENTS_ARENA_BYTES=1024u GAME_EVENTS_LOG_CAP=64
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_overflow PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_overflow COMMAND test_game_events_overflow)
    set_tests_properties(test_game_events_overflow PROPERTIES LABELS "core")

    # A4: round-trip gate for the generated fragment state layer. Links the
    # generated game_state.c (data + static wrappers, no game_save_* calls) so
    # game_save.c is NOT needed and the engine is not pulled in. The generated
    # source is an add_custom_command OUTPUT, so regen is an automatic build
    # prerequisite. GAME_STATE_GENERATED_* is always defined (state is always on).
    add_executable(test_game_state_roundtrip
        tests/test_game_state_roundtrip.c
        "${GAME_STATE_GENERATED_SOURCE}" "${GAME_STATE_SRC}/game_state_json.c")
    target_link_libraries(test_game_state_roundtrip PRIVATE cjson unity)
    target_include_directories(test_game_state_roundtrip PRIVATE src "${GAME_STATE_GENERATED_DIR}")
    target_compile_definitions(test_game_state_roundtrip PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_state_roundtrip PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_state_roundtrip COMMAND test_game_state_roundtrip)
    set_tests_properties(test_game_state_roundtrip PROPERTIES LABELS "core")

    # E2: typed event layer round-trip over the COMMITTED golden mini events + frozen
    # E1 transport (no build-time generation). Unconditional -- the golden is a
    # committed fixture and game_events.{c,h}/game_event_desc.h are always compiled.
    # This double-serves as a compile check of the golden.
    add_executable(test_game_events_typed
        tests/test_game_events_typed.c
        "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_events_typed PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events_typed PRIVATE
        "${GAME_EVENTS_INC}"
        src
        "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_events_typed PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_typed PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_typed COMMAND test_game_events_typed)
    set_tests_properties(test_game_events_typed PROPERTIES LABELS "core")

    # E3: descriptor-driven renderer over the COMMITTED golden mini events + frozen E1
    # transport. Native, no devapi (renderer is pure). Unconditional (golden is committed).
    # nt_hash inherits the preset's NT_HASH_LABELS -> asserts are label-agnostic (HIGH-1):
    # devapi-debug exercises the label branch, native-debug the hex branch.
    add_executable(test_game_event_render
        tests/test_game_event_render.c
        "${GAME_EVENTS_SRC}/game_event_render.c"
        "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_event_render PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_event_render PRIVATE
        "${GAME_EVENTS_INC}" src "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_event_render PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_event_render PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_event_render COMMAND test_game_event_render)
    set_tests_properties(test_game_event_render PROPERTIES LABELS "core")

    # E4: analytics writer over the COMMITTED golden mini events + the built-in log type +
    # frozen E1 transport. Native, sink + clock injected (GAME_ANALYTICS_TESTING). Links
    # game_log.c (case #7 emits a log event). Label-agnostic asserts.
    add_executable(test_game_analytics
        tests/test_game_analytics.c
        "${GAME_EVENTS_SRC}/game_analytics.c"
        "${GAME_EVENTS_SRC}/game_event_render.c" # E3 renderer (reused)
        src/game_log.c            # case #7 game_log_emit
        "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_analytics PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_analytics PRIVATE
        "${GAME_EVENTS_INC}" src "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_analytics PRIVATE
        FEATURE_GAME_ANALYTICS=1 GAME_ANALYTICS_TESTING=1
        GAME_ANALYTICS_BUF_BYTES=256u        # small buffer -> threshold/drop are cheap
        GAME_ANALYTICS_FLUSH_BYTES=192u
        GAME_STORAGE_APP_ID="template_analytics_test"  # header app field
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_analytics PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_analytics COMMAND test_game_analytics)
    set_tests_properties(test_game_analytics PROPERTIES LABELS "core")

    add_executable(test_game_events_log_mirror
        tests/test_game_events_log_mirror.c
        "${GAME_EVENTS_SRC}/game_events_log_mirror.c"
        "${GAME_EVENTS_SRC}/game_event_render.c"
        "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_events_log_mirror PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_events_log_mirror PRIVATE
        "${GAME_EVENTS_INC}" src "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_events_log_mirror PRIVATE
        GAME_EVENTS_LOG_MIRROR=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_log_mirror PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_log_mirror COMMAND test_game_events_log_mirror)
    set_tests_properties(test_game_events_log_mirror PROPERTIES LABELS "core")

    # E2 (M3): warning-gated compile check of the richest generated branches
    # (i64/f64/hash/bool/bytes+len, union staging, offset arithmetic) live ONLY in
    # mini. An OBJECT lib compiles the golden mini events source under the SAME
    # -W set + -Werror as the game target, WITHOUT Unity (compile-only, no link) so
    # -Wconversion is not fighting the test framework's macros.
    add_library(check_mini_state_events OBJECT
        "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini/mini_state_events.gen.c")
    target_include_directories(check_mini_state_events PRIVATE
        "${GAME_EVENTS_INC}"
        src
        "${GAME_REPO_ROOT}/features/game-state/tests/golden/mini")
    target_link_libraries(check_mini_state_events PRIVATE nt_hash nt_log nt_core)  # headers only (OBJECT does not link)
    nt_set_warning_flags(check_mini_state_events)  # same -W set + -Werror toggle as the game target

    # The same stable Items header compiles against either a core-only or a
    # weapon-specific generated catalog. Outputs stay build-local.
    set(ITEMS_C_CATALOG_SCRIPT "${ITEMS_CORE_SCRIPTS}/items_c_catalog.py")
    set(ITEMS_C_CATALOG_SCRIPT_SOURCES
        "${ITEMS_C_CATALOG_SCRIPT}"
        "${ITEMS_CORE_SCRIPTS}/items_c_identifiers.py"
        "${ITEMS_CORE_SCRIPTS}/items_snapshot.py"
        "${ITEMS_CORE_SCRIPTS}/items_xxh64.py")
    set(ITEMS_API_PROOF_FIXTURES "${ITEMS_CORE_DIR}/tests/fixtures")
    set(ITEMS_API_CORE_ONLY_DIR "${CMAKE_BINARY_DIR}/generated/items-api-core-only")
    set(ITEMS_API_WEAPON_DIR "${CMAKE_BINARY_DIR}/generated/items-api-weapon")
    foreach(_variant core-only weapon)
        if(_variant STREQUAL "core-only")
            set(_snapshot "${ITEMS_API_PROOF_FIXTURES}/items_api_core_proof.json")
            set(_out_dir "${ITEMS_API_CORE_ONLY_DIR}")
        else()
            set(_snapshot "${ITEMS_API_PROOF_FIXTURES}/items_api_weapon_proof.json")
            set(_out_dir "${ITEMS_API_WEAPON_DIR}")
        endif()
        add_custom_command(
            OUTPUT
                "${_out_dir}/items_catalog.gen.h"
                "${_out_dir}/items_catalog.internal.gen.h"
                "${_out_dir}/items_catalog.gen.c"
                "${_out_dir}/items_catalog.luau"
            COMMAND "${Python3_EXECUTABLE}" "${ITEMS_C_CATALOG_SCRIPT}"
                --snapshot "${_snapshot}" --out-dir "${_out_dir}"
            DEPENDS ${ITEMS_C_CATALOG_SCRIPT_SOURCES} "${_snapshot}"
            COMMENT "Generating ${_variant} Items C catalog"
            VERBATIM)
    endforeach()

    add_executable(test_items_api_core_only
        tests/test_items_api_core_only.c
        "${ITEMS_CORE_SRC}/items_api.c"
        "${ITEMS_API_CORE_ONLY_DIR}/items_catalog.gen.c")
    target_link_libraries(test_items_api_core_only PRIVATE unity nt_hash nt_core)
    target_include_directories(test_items_api_core_only PRIVATE "${ITEMS_CORE_INC}" "${ITEMS_API_CORE_ONLY_DIR}")
    target_compile_definitions(test_items_api_core_only PRIVATE ITEMS_GAME_API_ENABLED=1 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_items_api_core_only)
    set_target_properties(test_items_api_core_only PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_api_core_only COMMAND test_items_api_core_only)
    set_tests_properties(test_items_api_core_only PROPERTIES LABELS "core")

    add_executable(test_items_api
        tests/test_items_api.c
        "${ITEMS_CORE_SRC}/items_api.c"
        "${ITEMS_API_WEAPON_DIR}/items_catalog.gen.c")
    target_link_libraries(test_items_api PRIVATE unity nt_hash nt_core)
    target_include_directories(test_items_api PRIVATE "${ITEMS_CORE_INC}" "${ITEMS_API_WEAPON_DIR}")
    target_compile_definitions(test_items_api PRIVATE ITEMS_GAME_API_ENABLED=1 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_items_api)
    set_target_properties(test_items_api PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_api COMMAND test_items_api)
    set_tests_properties(test_items_api PROPERTIES LABELS "core")

    add_test(NAME items_c_catalog_test
        COMMAND "${Python3_EXECUTABLE}" "${ITEMS_CORE_SCRIPTS}/items_c_catalog_test.py")
    set_tests_properties(items_c_catalog_test PROPERTIES LABELS "core")

    # Ownership tests link the same generated catalog the game compiles.
    function(configure_items_runtime_catalog_test target_name)
        add_dependencies(${target_name} items_catalog_gen)
        target_sources(${target_name} PRIVATE
            "${ITEMS_CORE_SRC}/items_api.c"
            "${ITEMS_CATALOG_SOURCE}")
        target_include_directories(${target_name} PRIVATE
            tests "${ITEMS_CATALOG_BUILD_DIR}")
    endfunction()

    # Items fragment round-trip links generated state/events, game-owned hooks,
    # JSON/event plumbing, ownership core, and the compact runtime catalog.
    # game_save.c is not linked; the test TU stubs game_save_mark_dirty.
    add_executable(test_items_fragment
        tests/test_items_fragment.c
        "${ITEMS_STATE_GENERATED_SOURCE}"
        "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        "${ITEMS_CORE_SRC}/items_reconcile.c"
        "${ITEMS_CORE_SRC}/items_containers.c"
        "${GAME_STATE_SRC}/game_state_json.c"
        "${GAME_EVENTS_SRC}/game_events.c"
        "${ENGINE_DIR}/tests/unit/test_helpers/nt_assert_trap.c")
    configure_items_runtime_catalog_test(test_items_fragment)
    target_link_libraries(test_items_fragment PRIVATE cjson unity nt_hash nt_log nt_core)
    target_include_directories(test_items_fragment PRIVATE "${ITEMS_CORE_INC}" "${GAME_EVENTS_INC}" src "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}" "${ENGINE_DIR}/tests/unit")
    target_compile_definitions(test_items_fragment PRIVATE ITEMS_RUNTIME_TESTING=1 _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_items_fragment PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_fragment COMMAND test_items_fragment
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    set_tests_properties(test_items_fragment PROPERTIES LABELS "core")

    # The final production profile may compile assertions out entirely. Keep the
    # complete ownership suite on that exact code path so runtime work never
    # depends on side effects or validation hidden inside NT_ASSERT.
    add_executable(test_items_fragment_assert_off
        tests/test_items_fragment.c
        "${ITEMS_STATE_GENERATED_SOURCE}"
        "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        "${ITEMS_CORE_SRC}/items_reconcile.c"
        "${ITEMS_CORE_SRC}/items_containers.c"
        "${GAME_STATE_SRC}/game_state_json.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    configure_items_runtime_catalog_test(test_items_fragment_assert_off)
    target_link_libraries(test_items_fragment_assert_off PRIVATE cjson unity nt_hash nt_log nt_core)
    target_include_directories(test_items_fragment_assert_off PRIVATE "${ITEMS_CORE_INC}" "${GAME_EVENTS_INC}" src "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}" "${ENGINE_DIR}/tests/unit")
    target_compile_definitions(test_items_fragment_assert_off PRIVATE ITEMS_RUNTIME_TESTING=1 NT_ASSERT_MODE=0 _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_items_fragment_assert_off PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_fragment_assert_off COMMAND test_items_fragment_assert_off
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    set_tests_properties(test_items_fragment_assert_off PROPERTIES LABELS "core")

    # Keep the shipped Lua catalog and release receipt on the normal ctest path.
    add_test(NAME items_catalog_validate COMMAND "${Python3_EXECUTABLE}"
        "${ITEMS_CORE_SCRIPTS}/items_cli.py"
        --project-root "${CMAKE_CURRENT_SOURCE_DIR}" validate)
    set_tests_properties(items_catalog_validate PROPERTIES LABELS "core")

    # test_progression compiles progression.c against the real generated state.
    # #includes progression_tracks.gen.h -- but does NOT link
    # progression_tracks.gen.c (it links its OWN hand-written k_tracks catalog,
    # tests/test_progression_catalog.c, to avoid a duplicate-symbol link error,
    # R10). Without a linked OUTPUT of the codegen custom_command, ninja has no
    # dependency edge forcing the .gen.h to exist before progression.c compiles
    # on a clean/parallel build -- a phony target + add_dependencies closes that
    # gap; items already links its generated source and has the dependency edge.
    add_custom_target(progression_tracks_gen DEPENDS
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.h"
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c")

    add_executable(test_progression
        tests/test_progression.c
        tests/test_progression_catalog.c                        # РУКОПИСНЫЙ k_tracks (НЕ .gen.c, R10)
        "${PROGRESSION_CORE_SRC}/progression.c"                   # T0337 M2: in-place module
        "${PROGRESSION_STATE_GENERATED_SOURCE}"                  # progression_state.c
        "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}"           # progression_state_events.gen.c
        "${ITEMS_CORE_SRC}/items_containers.c"                    # items runtime (progression spends/reads purse; T0337 M1: in-place module)
        "${ITEMS_STATE_GENERATED_SOURCE}" "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        "${ITEMS_CORE_SRC}/items_reconcile.c"                     # items reconcile (T0337 M1 split, link completeness)
        "${GAME_STATE_SRC}/game_state_json.c" "${GAME_EVENTS_SRC}/game_events.c")
    add_dependencies(test_progression progression_tracks_gen)   # H2: guarantees progression_tracks.gen.h before progression.c compiles
    configure_items_runtime_catalog_test(test_progression)
    target_link_libraries(test_progression PRIVATE cjson unity nt_hash nt_log nt_core)
    target_include_directories(test_progression PRIVATE "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${GAME_EVENTS_INC}" src "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_progression PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_progression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_progression COMMAND test_progression WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    set_tests_properties(test_progression PROPERTIES LABELS "core")

    # GOLDEN: links the demo progression_tracks.gen.c.
    # The custom-command output provides the dependency edge; this is the
    # ONLY target in this file that links the real generated catalog; no
    # duplicate-k_tracks conflict with test_progression (which never links it).
    add_executable(test_progression_curve
        tests/test_progression_curve.c
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c")
    add_dependencies(test_progression_curve items_catalog_gen)
    target_link_libraries(test_progression_curve PRIVATE unity)
    # Both module include roots are required because progression.h includes
    # features/items/items.h, which in turn includes the generated catalog
    # header; keep all three on this target.
    target_include_directories(test_progression_curve PRIVATE "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" src "${GAME_SOURCE_GENERATED_DIR}" "${ITEMS_CATALOG_BUILD_DIR}")
    target_compile_definitions(test_progression_curve PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_progression_curve PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_progression_curve COMMAND test_progression_curve)
    set_tests_properties(test_progression_curve PROPERTIES LABELS "core")

    # L0 int64-abbreviation formatter: pure, no generated-file/state
    # dependency, so a plain two-file test target (precedent test_game_state_json).
    add_executable(test_game_format tests/test_game_format.c src/game_format.c)
    target_link_libraries(test_game_format PRIVATE unity)
    target_include_directories(test_game_format PRIVATE src)
    target_compile_definitions(test_game_format PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_format PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_format COMMAND test_game_format)
    set_tests_properties(test_game_format PROPERTIES LABELS "core")

    # Localization is proven at the seam, not on either side of it: loc_test.py
    # checks what the generator writes, and test_loc_e2e runs that same
    # generator over the module's fixture corpus, compiles the result, and calls
    # it -- so a Python/C disagreement about the interpolation grammar or the
    # plural category sets is a red test instead of a wrong string on screen.
    add_test(NAME loc_generator_test
        COMMAND "${Python3_EXECUTABLE}" "${LOCALIZATION_SCRIPTS}/loc_test.py")
    set_tests_properties(loc_generator_test PROPERTIES LABELS "core")

    # The atlas repacks when the corpus changes, but a repack cannot invent a glyph the
    # TTF does not have: without this a new translation ships as boxes with every other
    # gate green. The template packs exactly one font, argv[2] of build_packs.c.
    if(NOT EMSCRIPTEN)
        add_test(NAME loc_font_charset_check
            COMMAND "${Python3_EXECUTABLE}" "${LOCALIZATION_SCRIPTS}/loc.py" fonts
                --strings "${CMAKE_CURRENT_SOURCE_DIR}/content/loc/strings.json"
                --font "game=${GAME_FONT_SOURCE}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(loc_font_charset_check PROPERTIES LABELS "core")
    endif()

    set(LOC_E2E_DIR "${CMAKE_BINARY_DIR}/generated/loc-e2e")
    set(LOC_E2E_STRINGS "${LOCALIZATION_DIR}/tests/e2e_strings.json")
    add_custom_command(
        OUTPUT "${LOC_E2E_DIR}/loc_strings.gen.h" "${LOC_E2E_DIR}/loc_strings.gen.c"
               "${LOC_E2E_DIR}/loc_keys.gen.json" "${LOC_E2E_DIR}/loc_charset.gen.h"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LOC_E2E_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${LOC_GENERATOR}" generate
            --strings "${LOC_E2E_STRINGS}" --out-dir "${LOC_E2E_DIR}"
        DEPENDS "${LOC_E2E_STRINGS}" "${LOC_GENERATOR}"
        COMMENT "Generating localization e2e fixture table"
        VERBATIM)
    add_custom_command(
        OUTPUT "${LOC_E2E_DIR}/loc_plural_cases.gen.h" "${LOC_E2E_DIR}/loc_plural_cases.gen.c"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LOC_E2E_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${LOC_GENERATOR}" plural-cases --out-dir "${LOC_E2E_DIR}"
        DEPENDS "${LOC_GENERATOR}"
        COMMENT "Generating localization plural cross-check cases"
        VERBATIM)
    add_executable(test_loc_e2e
        "${LOCALIZATION_DIR}/tests/test_loc_e2e.c"
        "${LOC_E2E_DIR}/loc_strings.gen.c"
        "${LOC_E2E_DIR}/loc_plural_cases.gen.c"
        "${LOCALIZATION_SRC}/loc.c")
    target_link_libraries(test_loc_e2e PRIVATE unity nt_mem_scratch nt_log nt_core)
    target_include_directories(test_loc_e2e PRIVATE
        "${LOCALIZATION_INC}" "${LOC_E2E_DIR}" "${ENGINE_DIR}/engine")
    target_compile_definitions(test_loc_e2e PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_loc_e2e PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_loc_e2e COMMAND test_loc_e2e)
    set_tests_properties(test_loc_e2e PROPERTIES LABELS "core")

    add_executable(test_platform_sdk
        tests/test_platform_sdk.c
        "${PLATFORM_SDK_SRC}/platform_sdk.c")
    target_link_libraries(test_platform_sdk PRIVATE unity)
    target_include_directories(test_platform_sdk PRIVATE "${PLATFORM_SDK_INC}")
    target_compile_definitions(test_platform_sdk PRIVATE
        PLATFORM_SDK_TARGET_ID=${GAME_PLATFORM_TARGET_ID}
        PLATFORM_SDK_CURRENT_ID=${GAME_PLATFORM_SDK_ID}
        PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED=${GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED}
        PLATFORM_SDK_ADS_SUPPORTED=${GAME_PLATFORM_ADS_SUPPORTED}
        PLATFORM_SDK_REWARDED_SUPPORTED=${GAME_PLATFORM_REWARDED_SUPPORTED}
        PLATFORM_SDK_STORAGE_SUPPORTED=${GAME_PLATFORM_STORAGE_SUPPORTED}
        PLATFORM_SDK_TESTING=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_platform_sdk PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_platform_sdk COMMAND test_platform_sdk)
    set_tests_properties(test_platform_sdk PROPERTIES LABELS "core")

    add_executable(test_focus_prompt tests/test_focus_prompt.c)
    target_link_libraries(test_focus_prompt PRIVATE unity)
    target_include_directories(test_focus_prompt PRIVATE src)
    target_compile_definitions(test_focus_prompt PRIVATE _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_focus_prompt)
    set_target_properties(test_focus_prompt PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_focus_prompt COMMAND test_focus_prompt)
    set_tests_properties(test_focus_prompt PROPERTIES LABELS "core")

    add_executable(test_game_input
        tests/test_game_input.c
        src/game_input.c)
    target_link_libraries(test_game_input PRIVATE unity nt_input_stub)
    target_include_directories(test_game_input PRIVATE src)
    target_compile_definitions(test_game_input PRIVATE
        _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_game_input)
    set_target_properties(test_game_input PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_input COMMAND test_game_input)
    set_tests_properties(test_game_input PROPERTIES LABELS "core")

    add_executable(test_game_asset_paths
        tests/test_game_asset_paths.c
        src/game_asset_paths.c)
    target_link_libraries(test_game_asset_paths PRIVATE unity)
    target_include_directories(test_game_asset_paths PRIVATE src)
    target_compile_definitions(test_game_asset_paths PRIVATE _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_game_asset_paths)
    set_target_properties(test_game_asset_paths PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_asset_paths COMMAND test_game_asset_paths)
    set_tests_properties(test_game_asset_paths PROPERTIES LABELS "core")

    add_executable(test_platform_lifecycle
        tests/test_platform_lifecycle.c
        src/platform_lifecycle.c
        "${PLATFORM_SDK_SRC}/platform_sdk.c")
    target_link_libraries(test_platform_lifecycle PRIVATE unity)
    target_include_directories(test_platform_lifecycle PRIVATE src "${PLATFORM_SDK_INC}")
    target_compile_definitions(test_platform_lifecycle PRIVATE
        PLATFORM_SDK_TARGET_ID=${GAME_PLATFORM_TARGET_ID}
        PLATFORM_SDK_CURRENT_ID=${GAME_PLATFORM_SDK_ID}
        PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED=${GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED}
        PLATFORM_SDK_ADS_SUPPORTED=${GAME_PLATFORM_ADS_SUPPORTED}
        PLATFORM_SDK_REWARDED_SUPPORTED=${GAME_PLATFORM_REWARDED_SUPPORTED}
        PLATFORM_SDK_STORAGE_SUPPORTED=${GAME_PLATFORM_STORAGE_SUPPORTED}
        PLATFORM_SDK_TESTING=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_platform_lifecycle PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_platform_lifecycle COMMAND test_platform_lifecycle)
    set_tests_properties(test_platform_lifecycle PROPERTIES LABELS "core")

    add_executable(test_platform_sdk_events
        tests/test_platform_sdk_events.c
        "${PLATFORM_SDK_SRC}/platform_sdk.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_platform_sdk_events PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_platform_sdk_events PRIVATE "${PLATFORM_SDK_INC}" "${GAME_EVENTS_INC}")
    target_compile_definitions(test_platform_sdk_events PRIVATE
        PLATFORM_SDK_TARGET_ID=${GAME_PLATFORM_TARGET_ID}
        PLATFORM_SDK_CURRENT_ID=${GAME_PLATFORM_SDK_ID}
        PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED=${GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED}
        PLATFORM_SDK_ADS_SUPPORTED=${GAME_PLATFORM_ADS_SUPPORTED}
        PLATFORM_SDK_REWARDED_SUPPORTED=${GAME_PLATFORM_REWARDED_SUPPORTED}
        PLATFORM_SDK_STORAGE_SUPPORTED=${GAME_PLATFORM_STORAGE_SUPPORTED}
        PLATFORM_SDK_TESTING=1
        FEATURE_GAME_EVENTS=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_platform_sdk_events PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_platform_sdk_events COMMAND test_platform_sdk_events)
    set_tests_properties(test_platform_sdk_events PROPERTIES LABELS "core")

    find_program(Node_EXECUTABLE node REQUIRED)
    if(Node_EXECUTABLE)
        add_test(NAME scenes_core_consumer_contract
            COMMAND "${Node_EXECUTABLE}" --test
                features/scenes-core/tests/test_consumer_scene_contracts.mjs
            WORKING_DIRECTORY "${GAME_REPO_ROOT}")
        set_tests_properties(scenes_core_consumer_contract PROPERTIES LABELS "core")
        add_test(NAME scenes_core_tooling_contract
            COMMAND "${Node_EXECUTABLE}" --test
                features/scenes-core/tests/test_scaffold_scene.mjs
                features/scenes-core/tests/test_scene_devapi_schema.mjs
            WORKING_DIRECTORY "${GAME_REPO_ROOT}")
        set_tests_properties(scenes_core_tooling_contract PROPERTIES LABELS "core")
        set_tests_properties(scenes_core_tooling_contract PROPERTIES
            ENVIRONMENT "SCENE_TEST_CC=${CMAKE_C_COMPILER}")
        if(GAME_DEVAPI_ENABLED)
            add_test(NAME scenes_core_devapi_runtime_schema
                COMMAND "${Node_EXECUTABLE}" --test
                    features/scenes-core/tests/test_scene_devapi_runtime_schema.mjs
                WORKING_DIRECTORY "${GAME_REPO_ROOT}")
            set_tests_properties(scenes_core_devapi_runtime_schema PROPERTIES LABELS "core")
            set_tests_properties(scenes_core_devapi_runtime_schema PROPERTIES
                ENVIRONMENT
                    "SCENE_DEVAPI_SCHEMA_FIXTURE=$<TARGET_FILE:test_scenes_core_devapi>")
        endif()
        add_test(NAME platform_sdk_node_test
            COMMAND "${Node_EXECUTABLE}" --test features/platform-sdk/tests/platform_sdk.test.mjs
            WORKING_DIRECTORY "${GAME_REPO_ROOT}")
        set_tests_properties(platform_sdk_node_test PROPERTIES LABELS "core")
    endif()

    # T0327 tail: 4-fragment composition test -- lifts settings/items/progression/game
    # through the REAL game_save registry (envelope + on_new_game fan-out + skip-reset).
    # GAME_SAVE_TESTING injects clocks & avoids nt_time (precedent test_game_save).
    add_executable(test_template_composition
        tests/test_template_composition.c
        src/game_items.c
        "${GAME_STATE_SRC}/game_save.c" "${GAME_STATE_SRC}/game_save_writer.c" "${GAME_STATE_SRC}/game_storage.c"
        "${GAME_STATE_SRC}/game_storage_backend_native.c"
        "${GAME_STATE_SRC}/game_state_json.c" "${GAME_EVENTS_SRC}/game_events.c"
        "${GAME_STATE_GENERATED_SOURCE}" "${GAME_STATE_GENERATED_EVENTS_SOURCE}"
        "${SETTINGS_STATE_GENERATED_SOURCE}" src/features/settings/settings.c
        "${ITEMS_STATE_GENERATED_SOURCE}" "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        "${ITEMS_CORE_SRC}/items_reconcile.c" "${ITEMS_CORE_SRC}/items_containers.c"
        "${PROGRESSION_STATE_GENERATED_SOURCE}" "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}"
        "${PROGRESSION_CORE_SRC}/progression.c"
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c"   # REAL hero curve (cf. test_progression_curve)
        # settings.c binds the persisted language to the string table, so the
        # composition links the real module and the real corpus.
        "${LOCALIZATION_SRC}/loc.c"
        "${GAME_SOURCE_GENERATED_DIR}/loc_strings.gen.c")
    add_dependencies(test_template_composition progression_tracks_gen)  # progression.c #includes .gen.h
    configure_items_runtime_catalog_test(test_template_composition)
    # nt_ui_interface (review #1 smoke-check): settings.h pulls ui/nt_ui.h for
    # nt_ui_context_t; header-only include-root + NT_UI_DEBUG_TOOLS define, no
    # Clay/impl chain -- draw_ui is never called in this TU, only declared.
    target_link_libraries(test_template_composition PRIVATE cjson unity nt_hash nt_log nt_core nt_ui_interface nt_mem_scratch)
    if(GAME_DEVAPI_ENABLED)
        target_sources(test_template_composition PRIVATE "${GAME_STATE_SRC}/game_save_devapi.c")
        target_link_libraries(test_template_composition PRIVATE nt_devapi_default nt_app_stub)
        target_compile_definitions(test_template_composition PRIVATE NT_DEVAPI_ENABLED=1)
    else()
        target_compile_definitions(test_template_composition PRIVATE NT_DEVAPI_ENABLED=0)
    endif()
    target_include_directories(test_template_composition PRIVATE
        "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${GAME_EVENTS_INC}" src
        "${LOCALIZATION_INC}"
        "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_template_composition PRIVATE
        GAME_SAVE_TESTING=1 GAME_ITEMS_TESTING=1 GAME_STORAGE_APP_ID="template_composition_test"
        GAME_STORAGE_NATIVE_ROOT="${CMAKE_BINARY_DIR}/tests/build/composition"
        GAME_SAVE_AUTOSAVE_SLOT="test_composition"
        GAME_SAVE_DEBOUNCE_MS=2000 GAME_SAVE_MAX_INTERVAL_MS=30000 GAME_SAVE_DOC_VERSION=2
        ITEMS_LEGACY_SAVE_V1_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/items_save_v1.json"
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_template_composition PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_template_composition COMMAND test_template_composition
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    set_tests_properties(test_template_composition PROPERTIES LABELS "core")

    # Engine static libraries are sanitizer-instrumented in Debug. Every test
    # executable that can link them must carry the matching runtime at link time.
    set(GAME_NATIVE_TEST_TARGETS
        test_audio_core test_audio_resource test_audio_backend_native test_game_audio
        test_game_state_json test_game_state_nested test_game_storage
        test_game_storage_write_modes
        test_game_storage_web_backend test_game_save_blocked test_game_save
        test_game_events test_game_events_overflow test_game_state_roundtrip
        test_game_events_typed test_game_event_render test_game_analytics
        test_game_events_log_mirror test_items_api_core_only
        test_items_api
        test_items_fragment test_items_fragment_assert_off test_progression test_progression_curve
        test_game_format test_loc_e2e test_platform_sdk test_game_input test_platform_lifecycle
        test_platform_sdk_events test_template_composition
        test_scenes_core_catalog test_scenes_core_lifecycle
        test_scenes_core_navigation test_scenes_core_ordering
        test_scenes_core_presentation test_scenes_core_deadlines
        test_scenes_core_reentrancy test_scenes_core_devapi_disabled)
    if(GAME_DEVAPI_ENABLED)
        list(APPEND GAME_NATIVE_TEST_TARGETS test_scenes_core_devapi)
    endif()
    foreach(_test_target IN LISTS GAME_NATIVE_TEST_TARGETS)
        target_sources(${_test_target} PRIVATE
            "${GAME_STATE_SRC}/game_save_writer.c"
            "${GAME_STATE_SRC}/game_save_text.c")
        nt_set_sanitizer_flags(${_test_target})
    endforeach()
endif()

if(EMSCRIPTEN)
    enable_testing()
    add_executable(test_scenes_core_web_smoke
        "${SCENES_CORE_DIR}/tests/test_scene_manager_web_smoke.c"
        "${SCENES_CORE_SRC}/scene_manager.c")
    target_include_directories(test_scenes_core_web_smoke PRIVATE
        "${SCENES_CORE_INC}")
    target_compile_definitions(test_scenes_core_web_smoke PRIVATE
        _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_scenes_core_web_smoke)
    set_target_properties(test_scenes_core_web_smoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_scenes_core_web_smoke
        COMMAND test_scenes_core_web_smoke)
    set_tests_properties(test_scenes_core_web_smoke PROPERTIES LABELS "core")

    find_program(SCENES_Node_EXECUTABLE node REQUIRED)
    if(SCENES_Node_EXECUTABLE)
        add_test(NAME scenes_core_consumer_contract
            COMMAND "${SCENES_Node_EXECUTABLE}" --test
                features/scenes-core/tests/test_consumer_scene_contracts.mjs
            WORKING_DIRECTORY "${GAME_REPO_ROOT}")
        set_tests_properties(scenes_core_consumer_contract PROPERTIES LABELS "core")
        add_test(NAME scenes_core_tooling_contract
            COMMAND "${SCENES_Node_EXECUTABLE}" --test
                features/scenes-core/tests/test_scaffold_scene.mjs
                features/scenes-core/tests/test_scene_devapi_schema.mjs
            WORKING_DIRECTORY "${GAME_REPO_ROOT}")
        set_tests_properties(scenes_core_tooling_contract PROPERTIES LABELS "core")
        set_tests_properties(scenes_core_tooling_contract PROPERTIES
            ENVIRONMENT "SCENE_TEST_CC=${CMAKE_C_COMPILER}")
    endif()
endif()

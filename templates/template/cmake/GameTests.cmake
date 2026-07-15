# --- native C unit tests (Unity + CTest); introduced in A1, extended in A2/A3 ---
if(NOT EMSCRIPTEN)
    enable_testing()
    set(AUDIO_CORE_TESTS "${AUDIO_CORE_DIR}/tests")
    add_library(audio_unity STATIC "${ENGINE_DIR}/deps/unity/src/unity.c")
    target_include_directories(audio_unity PUBLIC "${ENGINE_DIR}/deps/unity/src")
    target_compile_definitions(audio_unity PRIVATE _CRT_SECURE_NO_WARNINGS)

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

    add_executable(test_audio_resource
        "${AUDIO_CORE_TESTS}/test_audio_resource.c"
        "${AUDIO_CORE_SRC}/audio_resource.c")
    target_link_libraries(test_audio_resource PRIVATE unity nt_shared)
    target_include_directories(test_audio_resource PRIVATE "${AUDIO_CORE_SRC}" "${ENGINE_DIR}/engine")
    target_compile_definitions(test_audio_resource PRIVATE NT_INTROSPECT_ENABLED=0 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_audio_resource)
    set_target_properties(test_audio_resource PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_audio_resource COMMAND test_audio_resource)

    add_executable(test_audio_backend_native
        "${AUDIO_CORE_TESTS}/test_audio_backend_native.c"
        "${AUDIO_CORE_SRC}/audio_backend_miniaudio.c"
        "${AUDIO_CORE_SRC}/audio_miniaudio_impl.c")
    target_link_libraries(test_audio_backend_native PRIVATE unity)
    target_include_directories(test_audio_backend_native PRIVATE "${AUDIO_CORE_SRC}" "${AUDIO_CORE_VENDOR}")
    target_compile_definitions(test_audio_backend_native PRIVATE
        AUDIO_MINIAUDIO_TEST_NO_DEVICE=1
        AUDIO_TEST_MP3_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/music/demo_jingle.mp3"
        _CRT_SECURE_NO_WARNINGS)
    audio_core_link_native_systems(test_audio_backend_native)
    nt_set_warning_flags(test_audio_backend_native)
    set_target_properties(test_audio_backend_native PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_audio_backend_native COMMAND test_audio_backend_native)

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

    find_program(AUDIO_NODE_EXECUTABLE node REQUIRED)
    add_test(NAME test_audio_web_library
        COMMAND "${AUDIO_NODE_EXECUTABLE}" --test "${AUDIO_CORE_TESTS}/test_audio_web_library.mjs"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")

    add_executable(test_game_state_json tests/test_game_state_json.c src/game_state_json.c)
    target_link_libraries(test_game_state_json PRIVATE cjson unity)
    target_include_directories(test_game_state_json PRIVATE src)
    target_compile_definitions(test_game_state_json PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_state_json PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_state_json COMMAND test_game_state_json)

    add_executable(test_game_storage tests/test_game_storage.c src/game_storage.c)
    # nt_log: game_storage.c warns via nt_log_warn on a read ERROR (nt_hash/nt_core are nt_log's deps).
    target_link_libraries(test_game_storage PRIVATE unity nt_log nt_core nt_hash)
    target_include_directories(test_game_storage PRIVATE src)
    target_compile_definitions(test_game_storage PRIVATE
        GAME_STORAGE_APP_ID="template_test" _CRT_SECURE_NO_WARNINGS)
    # MoveFileExA (native quarantine/atomic-replace) is in kernel32, linked by default.
    set_target_properties(test_game_storage PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_storage COMMAND test_game_storage
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    add_executable(test_game_save
        tests/test_game_save.c
        src/game_save.c src/game_storage.c src/game_state_json.c)
    # nt_log: game_save.c/game_storage.c warn via nt_log_warn on the read-error path.
    target_link_libraries(test_game_save PRIVATE cjson unity nt_log nt_core nt_hash)
    target_include_directories(test_game_save PRIVATE src)
    target_compile_definitions(test_game_save PRIVATE
        GAME_SAVE_TESTING=1
        GAME_STORAGE_APP_ID="template_test"
        GAME_SAVE_AUTOSAVE_SLOT="test_slot"
        GAME_SAVE_DEBOUNCE_MS=2000
        GAME_SAVE_MAX_INTERVAL_MS=30000
        GAME_SAVE_DOC_VERSION=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_save PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_save COMMAND test_game_save
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

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

    # A4: round-trip gate for the generated fragment state layer. Links the
    # generated game_state.c (data + static wrappers, no game_save_* calls) so
    # game_save.c is NOT needed and the engine is not pulled in. The generated
    # source is an add_custom_command OUTPUT, so regen is an automatic build
    # prerequisite. GAME_STATE_GENERATED_* is always defined (state is always on).
    add_executable(test_game_state_roundtrip
        tests/test_game_state_roundtrip.c
        "${GAME_STATE_GENERATED_SOURCE}" src/game_state_json.c)
    target_link_libraries(test_game_state_roundtrip PRIVATE cjson unity)
    target_include_directories(test_game_state_roundtrip PRIVATE src "${GAME_STATE_GENERATED_DIR}")
    target_compile_definitions(test_game_state_roundtrip PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_state_roundtrip PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_state_roundtrip COMMAND test_game_state_roundtrip)

    # E2: typed event layer round-trip over the COMMITTED golden mini events + frozen
    # E1 transport (no build-time generation). Unconditional -- the golden is a
    # committed fixture and game_events.{c,h}/game_event_desc.h are always compiled.
    # This double-serves as a compile check of the golden.
    add_executable(test_game_events_typed
        tests/test_game_events_typed.c
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_events_typed PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events_typed PRIVATE
        "${GAME_EVENTS_INC}"
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_events_typed PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_typed PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_typed COMMAND test_game_events_typed)

    # E3: descriptor-driven renderer over the COMMITTED golden mini events + frozen E1
    # transport. Native, no devapi (renderer is pure). Unconditional (golden is committed).
    # nt_hash inherits the preset's NT_HASH_LABELS -> asserts are label-agnostic (HIGH-1):
    # devapi-debug exercises the label branch, native-debug the hex branch.
    add_executable(test_game_event_render
        tests/test_game_event_render.c
        "${GAME_EVENTS_SRC}/game_event_render.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_event_render PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_event_render PRIVATE
        "${GAME_EVENTS_INC}" src "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_event_render PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_event_render PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_event_render COMMAND test_game_event_render)

    # E4: analytics writer over the COMMITTED golden mini events + the built-in log type +
    # frozen E1 transport. Native, sink + clock injected (GAME_ANALYTICS_TESTING). Links
    # game_log.c (case #7 emits a log event). Label-agnostic asserts.
    add_executable(test_game_analytics
        tests/test_game_analytics.c
        "${GAME_EVENTS_SRC}/game_analytics.c"
        "${GAME_EVENTS_SRC}/game_event_render.c" # E3 renderer (reused)
        src/game_log.c            # case #7 game_log_emit
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_analytics PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_analytics PRIVATE
        "${GAME_EVENTS_INC}" src "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_analytics PRIVATE
        FEATURE_GAME_ANALYTICS=1 GAME_ANALYTICS_TESTING=1
        GAME_ANALYTICS_BUF_BYTES=256u        # small buffer -> threshold/drop are cheap
        GAME_ANALYTICS_FLUSH_BYTES=192u
        GAME_STORAGE_APP_ID="template_test"  # header app field
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_analytics PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_analytics COMMAND test_game_analytics)

    add_executable(test_game_events_log_mirror
        tests/test_game_events_log_mirror.c
        "${GAME_EVENTS_SRC}/game_events_log_mirror.c"
        "${GAME_EVENTS_SRC}/game_event_render.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_game_events_log_mirror PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_events_log_mirror PRIVATE
        "${GAME_EVENTS_INC}" src "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_events_log_mirror PRIVATE
        GAME_EVENTS_LOG_MIRROR=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_log_mirror PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_log_mirror COMMAND test_game_events_log_mirror)

    # E2 (M3): warning-gated compile check of the richest generated branches
    # (i64/f64/hash/bool/bytes+len, union staging, offset arithmetic) live ONLY in
    # mini. An OBJECT lib compiles the golden mini events source under the SAME
    # -W set + -Werror as the game target, WITHOUT Unity (compile-only, no link) so
    # -Wconversion is not fighting the test framework's macros.
    add_library(check_mini_state_events OBJECT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c")
    target_include_directories(check_mini_state_events PRIVATE
        "${GAME_EVENTS_INC}"
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_link_libraries(check_mini_state_events PRIVATE nt_hash nt_log nt_core)  # headers only (OBJECT does not link)
    nt_set_warning_flags(check_mini_state_events)  # same -W set + -Werror toggle as the game target

    # Catalog lookup over the generated const tables.
    add_executable(test_items_catalog
        tests/test_items_catalog.c
        "${ITEMS_CORE_SRC}/items_catalog.c"
        "${ITEMS_CATALOG_GENERATED_SOURCE}")
    target_link_libraries(test_items_catalog PRIVATE unity)
    target_include_directories(test_items_catalog PRIVATE "${ITEMS_CORE_INC}" src "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_items_catalog PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_items_catalog PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_catalog COMMAND test_items_catalog)

    # T0364 proof: the same stable Items header compiles against either a
    # core-only or weapon-specific generated API. Outputs stay build-local.
    set(ITEMS_API_PROOF_SCRIPT "${ITEMS_CORE_SCRIPTS}/generate_items_api_proof.py")
    set(ITEMS_C_IDENTIFIERS "${ITEMS_CORE_SCRIPTS}/items_c_identifiers.py")
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
                "${_out_dir}/items_game.gen.h"
                "${_out_dir}/items_game.internal.gen.h"
                "${_out_dir}/items_game.gen.c"
                "${_out_dir}/items_game.luau"
            COMMAND "${Python3_EXECUTABLE}" "${ITEMS_API_PROOF_SCRIPT}"
                --snapshot "${_snapshot}" --out-dir "${_out_dir}"
            DEPENDS "${ITEMS_API_PROOF_SCRIPT}" "${ITEMS_C_IDENTIFIERS}" "${_snapshot}"
            COMMENT "Generating ${_variant} Items API proof"
            VERBATIM)
    endforeach()

    add_executable(test_items_api_core_only
        tests/test_items_api_core_only.c
        "${ITEMS_CORE_SRC}/items_api.c"
        "${ITEMS_API_CORE_ONLY_DIR}/items_game.gen.c")
    target_link_libraries(test_items_api_core_only PRIVATE unity nt_hash nt_core)
    target_include_directories(test_items_api_core_only PRIVATE "${ITEMS_CORE_INC}" "${ITEMS_API_CORE_ONLY_DIR}")
    target_compile_definitions(test_items_api_core_only PRIVATE ITEMS_GAME_API_ENABLED=1 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_items_api_core_only)
    set_target_properties(test_items_api_core_only PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_api_core_only COMMAND test_items_api_core_only)

    add_executable(test_items_api
        tests/test_items_api.c
        "${ITEMS_CORE_SRC}/items_api.c"
        "${ITEMS_API_WEAPON_DIR}/items_game.gen.c")
    target_link_libraries(test_items_api PRIVATE unity nt_hash nt_core)
    target_include_directories(test_items_api PRIVATE "${ITEMS_CORE_INC}" "${ITEMS_API_WEAPON_DIR}")
    target_compile_definitions(test_items_api PRIVATE ITEMS_GAME_API_ENABLED=1 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_items_api)
    set_target_properties(test_items_api PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_api COMMAND test_items_api)

    add_test(NAME generate_items_api_proof_test
        COMMAND "${Python3_EXECUTABLE}" "${ITEMS_CORE_SCRIPTS}/generate_items_api_proof_test.py")

    # T0365 native proof: bind the exact Python-generated compact package.
    set(ITEMS_RUNTIME_PACKAGE_DIR "${CMAKE_BINARY_DIR}/generated/items-runtime")
    set(ITEMS_RUNTIME_PACKAGE_BLOB "${ITEMS_RUNTIME_PACKAGE_DIR}/items.catalog")
    set(ITEMS_RUNTIME_PACKAGE_HEADER "${ITEMS_RUNTIME_PACKAGE_DIR}/items_catalog_abi.gen.h")
    set(ITEMS_RUNTIME_PACKAGE_SNAPSHOT "${ITEMS_CORE_DIR}/tests/fixtures/items_runtime_snapshot_v1.json")
    add_custom_command(
        OUTPUT "${ITEMS_RUNTIME_PACKAGE_BLOB}" "${ITEMS_RUNTIME_PACKAGE_HEADER}"
        COMMAND "${Python3_EXECUTABLE}" "${ITEMS_CORE_SCRIPTS}/items_runtime_package.py" build
            --snapshot "${ITEMS_RUNTIME_PACKAGE_SNAPSHOT}"
            --out "${ITEMS_RUNTIME_PACKAGE_BLOB}"
            --header-out "${ITEMS_RUNTIME_PACKAGE_HEADER}"
        DEPENDS
            "${ITEMS_CORE_SCRIPTS}/items_runtime_package.py"
            "${ITEMS_CORE_SCRIPTS}/generate_items_api_proof.py"
            "${ITEMS_C_IDENTIFIERS}"
            "${ITEMS_RUNTIME_PACKAGE_SNAPSHOT}"
        COMMENT "Generating compact Items runtime package proof"
        VERBATIM)
    add_custom_target(items_runtime_package_gen DEPENDS
        "${ITEMS_RUNTIME_PACKAGE_BLOB}" "${ITEMS_RUNTIME_PACKAGE_HEADER}")
    add_executable(test_items_runtime_package
        tests/test_items_runtime_package.c
        "${ITEMS_CORE_SRC}/items_runtime_package.c")
    add_dependencies(test_items_runtime_package items_runtime_package_gen)
    target_link_libraries(test_items_runtime_package PRIVATE unity nt_hash nt_core)
    target_include_directories(test_items_runtime_package PRIVATE
        "${ITEMS_CORE_INC}" "${ITEMS_RUNTIME_PACKAGE_DIR}")
    target_compile_definitions(test_items_runtime_package PRIVATE
        ITEMS_RUNTIME_PACKAGE_ENABLED=1
        ITEMS_RUNTIME_PACKAGE_PATH="${ITEMS_RUNTIME_PACKAGE_BLOB}"
        _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_items_runtime_package)
    set_target_properties(test_items_runtime_package PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_runtime_package COMMAND test_items_runtime_package)

    add_executable(test_items_runtime_resource
        tests/test_items_runtime_resource.c
        "${ITEMS_CORE_SRC}/items_runtime_package.c"
        "${ITEMS_CORE_SRC}/items_runtime_resource.c")
    add_dependencies(test_items_runtime_resource items_runtime_package_gen)
    target_link_libraries(test_items_runtime_resource PRIVATE unity nt_hash nt_core)
    target_include_directories(test_items_runtime_resource PRIVATE
        "${ITEMS_CORE_INC}" "${ITEMS_RUNTIME_PACKAGE_DIR}" "${ENGINE_DIR}/engine")
    target_compile_definitions(test_items_runtime_resource PRIVATE
        ITEMS_RUNTIME_PACKAGE_ENABLED=1
        ITEMS_RUNTIME_PACKAGE_PATH="${ITEMS_RUNTIME_PACKAGE_BLOB}"
        NT_INTROSPECT_ENABLED=0
        _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(test_items_runtime_resource)
    set_target_properties(test_items_runtime_resource PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_runtime_resource COMMAND test_items_runtime_resource)

    # T0365 benchmark candidates share one public API and equivalent two-item
    # data; only their storage/bind path differs.
    set(ITEMS_RUNTIME_BENCHMARK_DIR "${CMAKE_BINARY_DIR}/generated/items-runtime-benchmark")
    set(ITEMS_RUNTIME_BENCHMARK_FIXTURE
        "${ITEMS_CORE_DIR}/tests/fixtures/items_runtime_benchmark_arrays.json")
    add_custom_command(
        OUTPUT
            "${ITEMS_RUNTIME_BENCHMARK_DIR}/items_game.gen.h"
            "${ITEMS_RUNTIME_BENCHMARK_DIR}/items_game.internal.gen.h"
            "${ITEMS_RUNTIME_BENCHMARK_DIR}/items_game.gen.c"
            "${ITEMS_RUNTIME_BENCHMARK_DIR}/items_game.luau"
        COMMAND "${Python3_EXECUTABLE}" "${ITEMS_API_PROOF_SCRIPT}"
            --snapshot "${ITEMS_RUNTIME_BENCHMARK_FIXTURE}"
            --out-dir "${ITEMS_RUNTIME_BENCHMARK_DIR}"
        DEPENDS
            "${ITEMS_API_PROOF_SCRIPT}"
            "${ITEMS_C_IDENTIFIERS}"
            "${ITEMS_RUNTIME_BENCHMARK_FIXTURE}"
        COMMENT "Generating C-array Items runtime benchmark candidate"
        VERBATIM)
    add_custom_target(items_runtime_benchmark_arrays_gen DEPENDS
        "${ITEMS_RUNTIME_BENCHMARK_DIR}/items_game.gen.h"
        "${ITEMS_RUNTIME_BENCHMARK_DIR}/items_game.internal.gen.h"
        "${ITEMS_RUNTIME_BENCHMARK_DIR}/items_game.gen.c"
        "${ITEMS_RUNTIME_BENCHMARK_DIR}/items_game.luau")

    add_executable(benchmark_items_c_arrays
        "${ITEMS_CORE_DIR}/benchmarks/items_runtime_candidate.c"
        "${ITEMS_CORE_SRC}/items_api.c"
        "${ITEMS_RUNTIME_BENCHMARK_DIR}/items_game.gen.c")
    add_dependencies(benchmark_items_c_arrays items_runtime_benchmark_arrays_gen)
    target_link_libraries(benchmark_items_c_arrays PRIVATE nt_hash nt_core nt_time)
    target_include_directories(benchmark_items_c_arrays PRIVATE
        "${ITEMS_CORE_INC}" "${ITEMS_RUNTIME_BENCHMARK_DIR}")
    target_compile_definitions(benchmark_items_c_arrays PRIVATE
        ITEMS_GAME_API_ENABLED=1 ITEMS_BENCHMARK_RUNTIME=0 _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(benchmark_items_c_arrays)
    nt_set_sanitizer_flags(benchmark_items_c_arrays)
    set_target_properties(benchmark_items_c_arrays PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks")

    add_executable(benchmark_items_runtime_blob
        "${ITEMS_CORE_DIR}/benchmarks/items_runtime_candidate.c"
        "${ITEMS_CORE_SRC}/items_runtime_package.c")
    add_dependencies(benchmark_items_runtime_blob items_runtime_package_gen)
    target_link_libraries(benchmark_items_runtime_blob PRIVATE nt_hash nt_core nt_time)
    target_include_directories(benchmark_items_runtime_blob PRIVATE
        "${ITEMS_CORE_INC}" "${ITEMS_RUNTIME_PACKAGE_DIR}")
    target_compile_definitions(benchmark_items_runtime_blob PRIVATE
        ITEMS_RUNTIME_PACKAGE_ENABLED=1 ITEMS_BENCHMARK_RUNTIME=1
        ITEMS_RUNTIME_PACKAGE_PATH="${ITEMS_RUNTIME_PACKAGE_BLOB}"
        _CRT_SECURE_NO_WARNINGS)
    nt_set_warning_flags(benchmark_items_runtime_blob)
    nt_set_sanitizer_flags(benchmark_items_runtime_blob)
    set_target_properties(benchmark_items_runtime_blob PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks")

    # Items fragment round-trip links generated state/events, game-owned hooks,
    # JSON/event plumbing, ownership core, and the generated catalog.
    # game_save.c is not linked; the test TU stubs game_save_mark_dirty.
    add_executable(test_items_fragment
        tests/test_items_fragment.c
        "${ITEMS_STATE_GENERATED_SOURCE}"
        "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        src/features/items/items_bootstrap.c
        "${ITEMS_CORE_SRC}/items_reconcile.c"
        "${ITEMS_CORE_SRC}/items_containers.c"
        "${ITEMS_CORE_SRC}/items_catalog.c"
        "${ITEMS_CATALOG_GENERATED_SOURCE}"
        src/game_state_json.c
        "${GAME_EVENTS_SRC}/game_events.c")
    target_link_libraries(test_items_fragment PRIVATE cjson unity nt_hash nt_log nt_core)
    target_include_directories(test_items_fragment PRIVATE "${ITEMS_CORE_INC}" "${GAME_EVENTS_INC}" src "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_items_fragment PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_items_fragment PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_items_fragment COMMAND test_items_fragment
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

    # F1 (deep-review, T0327 И2c destructive-change guard follow-up): the
    # guard was VOLUNTARY until this line -- items_ops.py validate ran only
    # when a human remembered to invoke it by hand. Wiring it into ctest
    # means deleting a shipped def_id without a lock.removed reaction now
    # turns the BUILD's test suite red automatically; the forcing chain
    # The README Lock workflow starts at an automated gate, not a manual step.
    # Reads only committed source (content/items.json,
    # content/items.lock.json, state/items.schema.json) -- no generated-file
    # dependency, so no add_dependencies needed. WORKING_DIRECTORY is the
    # template root so the CWD-relative --catalog/--schema/--baseline/
    # --state-schema/--src-dir arguments resolve the same way a human running
    # the command from templates/template/ would see it.
    # T0337 M1 (H2/R7, CRITICAL): items_ops.py's own argparse defaults are
    # SCRIPT-relative (Path(__file__).parent.parent). After the move to
    # features/items-core/scripts/, those defaults would resolve into a
    # nonexistent features/items-core/content/ -- pass every path explicitly,
    # CWD-relative to the template root (WORKING_DIRECTORY below). Principle:
    # the module CLI takes paths ONLY from the caller (CWD/args), never from
    # __file__. --src-dir now points at the game-side items corner only
    # (src/features/items/) -- the display_name-keying lint no longer scans
    # the (relocated) ownership core; see items README L5-note.
    add_test(NAME items_ops_validate COMMAND "${Python3_EXECUTABLE}"
        "${ITEMS_CORE_SCRIPTS}/items_ops.py" validate
        --catalog content/items.json --schema content/item_fields.schema.json
        --baseline content/items.lock.json --state-schema state/items.schema.json
        --src-dir src/features/items
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    # F4 (deep-review): committed proof that the lock-workflow rules
    # themselves actually fire correctly (removed-without-reaction,
    # removed-version-not-shipped, lock-inconsistent, the F3 malformed-lock
    # IO error, the happy batch path) -- not just a manually re-run scratch
    # check. unittest against temp fixtures, precedent
    # features/game-state/scripts/generate_state_test.py.
    add_test(NAME items_ops_test COMMAND "${Python3_EXECUTABLE}" "${ITEMS_CORE_SCRIPTS}/items_ops_test.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

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
        "${ITEMS_CORE_SRC}/items_catalog.c" "${ITEMS_CATALOG_GENERATED_SOURCE}"
        "${ITEMS_STATE_GENERATED_SOURCE}" "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        src/features/items/items_bootstrap.c                     # items on_new_game (link completeness)
        "${ITEMS_CORE_SRC}/items_reconcile.c"                     # items reconcile (T0337 M1 split, link completeness)
        src/game_state_json.c "${GAME_EVENTS_SRC}/game_events.c")
    add_dependencies(test_progression progression_tracks_gen)   # H2: guarantees progression_tracks.gen.h before progression.c compiles
    target_link_libraries(test_progression PRIVATE cjson unity nt_hash nt_log nt_core)
    target_include_directories(test_progression PRIVATE "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${GAME_EVENTS_INC}" src "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_progression PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_progression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_progression COMMAND test_progression WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

    # GOLDEN: links the demo progression_tracks.gen.c.
    # The custom-command output provides the dependency edge; this is the
    # ONLY target in this file that links the real generated catalog; no
    # duplicate-k_tracks conflict with test_progression (which never links it).
    add_executable(test_progression_curve
        tests/test_progression_curve.c
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c")
    target_link_libraries(test_progression_curve PRIVATE unity)
    # Both module include roots are required because progression.h includes
    # features/items/items.h; keep both on this target.
    target_include_directories(test_progression_curve PRIVATE "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" src "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_progression_curve PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_progression_curve PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_progression_curve COMMAND test_progression_curve)

    # L0 int64-abbreviation formatter: pure, no generated-file/state
    # dependency, so a plain two-file test target (precedent test_game_state_json).
    add_executable(test_game_format tests/test_game_format.c src/game_format.c)
    target_link_libraries(test_game_format PRIVATE unity)
    target_include_directories(test_game_format PRIVATE src)
    target_compile_definitions(test_game_format PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_format PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_format COMMAND test_game_format)

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

    add_executable(test_platform_lifecycle
        tests/test_platform_lifecycle.c
        src/platform_lifecycle.c
        "${PLATFORM_SDK_SRC}/platform_sdk.c")
    target_link_libraries(test_platform_lifecycle PRIVATE unity nt_input_stub)
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

    find_program(Node_EXECUTABLE node)
    if(Node_EXECUTABLE)
        add_test(NAME platform_sdk_node_test
            COMMAND "${Node_EXECUTABLE}" --test features/platform-sdk/tests/platform_sdk.test.mjs
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    endif()

    # T0327 tail: 4-fragment composition test -- lifts settings/items/progression/game
    # through the REAL game_save registry (envelope + on_new_game fan-out + skip-reset).
    # GAME_SAVE_TESTING injects clocks & avoids nt_time (precedent test_game_save).
    add_executable(test_template_composition
        tests/test_template_composition.c
        src/game_save.c src/game_storage.c src/game_state_json.c "${GAME_EVENTS_SRC}/game_events.c"
        "${GAME_STATE_GENERATED_SOURCE}" "${GAME_STATE_GENERATED_EVENTS_SOURCE}"
        "${SETTINGS_STATE_GENERATED_SOURCE}" src/features/settings/settings.c
        "${ITEMS_STATE_GENERATED_SOURCE}" "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        src/features/items/items_bootstrap.c
        "${ITEMS_CORE_SRC}/items_reconcile.c" "${ITEMS_CORE_SRC}/items_containers.c"
        "${ITEMS_CORE_SRC}/items_catalog.c" "${ITEMS_CATALOG_GENERATED_SOURCE}"
        "${PROGRESSION_STATE_GENERATED_SOURCE}" "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}"
        "${PROGRESSION_CORE_SRC}/progression.c"
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c")   # REAL hero curve (cf. test_progression_curve)
    add_dependencies(test_template_composition progression_tracks_gen)  # progression.c #includes .gen.h
    # nt_ui_interface (review #1 smoke-check): settings.h pulls ui/nt_ui.h for
    # nt_ui_context_t; header-only include-root + NT_UI_DEBUG_TOOLS define, no
    # Clay/impl chain -- draw_ui is never called in this TU, only declared.
    target_link_libraries(test_template_composition PRIVATE cjson unity nt_hash nt_log nt_core nt_ui_interface)
    target_include_directories(test_template_composition PRIVATE
        "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${GAME_EVENTS_INC}" src
        "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_template_composition PRIVATE
        GAME_SAVE_TESTING=1 GAME_STORAGE_APP_ID="template_test"
        GAME_SAVE_AUTOSAVE_SLOT="test_composition"
        GAME_SAVE_DEBOUNCE_MS=2000 GAME_SAVE_MAX_INTERVAL_MS=30000 GAME_SAVE_DOC_VERSION=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_template_composition PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_template_composition COMMAND test_template_composition
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    # Engine static libraries are sanitizer-instrumented in Debug. Every test
    # executable that can link them must carry the matching runtime at link time.
    set(GAME_NATIVE_TEST_TARGETS
        test_audio_core test_audio_resource test_audio_backend_native test_game_audio
        test_game_state_json test_game_storage test_game_save
        test_game_events test_game_events_overflow test_game_state_roundtrip
        test_game_events_typed test_game_event_render test_game_analytics
        test_game_events_log_mirror test_items_catalog test_items_api_core_only
        test_items_api test_items_runtime_package test_items_runtime_resource test_items_fragment test_progression test_progression_curve
        test_game_format test_platform_sdk test_platform_lifecycle
        test_platform_sdk_events test_template_composition)
    foreach(_test_target IN LISTS GAME_NATIVE_TEST_TARGETS)
        nt_set_sanitizer_flags(${_test_target})
    endforeach()
endif()

set(_steam_platform_src "${CMAKE_CURRENT_LIST_DIR}/../src")

add_executable(test_steam_platform test_steam_platform.c)
target_link_libraries(test_steam_platform PRIVATE steam_platform unity)
target_include_directories(test_steam_platform PRIVATE "${_steam_platform_src}")
nt_set_warning_flags(test_steam_platform)
nt_set_sanitizer_flags(test_steam_platform)
# No APP_ID: this test never reaches Steam, it only needs the runtime library to load.
steam_platform_deploy(test_steam_platform)
add_test(NAME test_steam_platform COMMAND test_steam_platform)
set_tests_properties(test_steam_platform PROPERTIES LABELS "core")

if(STEAM_PLATFORM_HAS_SDK)
    add_executable(test_steam_platform_dispatch test_steam_platform_dispatch.cpp)
    target_compile_features(test_steam_platform_dispatch PRIVATE cxx_std_17)
    target_link_libraries(test_steam_platform_dispatch PRIVATE steam_platform steam_platform_steam_api unity)
    target_include_directories(test_steam_platform_dispatch PRIVATE "${_steam_platform_src}")
    nt_set_warning_flags(test_steam_platform_dispatch)
    steam_platform_deploy(test_steam_platform_dispatch)
    add_test(NAME test_steam_platform_dispatch COMMAND test_steam_platform_dispatch)
    set_tests_properties(test_steam_platform_dispatch PROPERTIES LABELS "core")

    # Talks to the developer's running Steam client as Spacewar (480): slow, and it shows the
    # account as playing Spacewar. Exit 77 is a skip. Its own folder, because a target deployed
    # without APP_ID removes steam_appid.txt from the folder it builds into.
    add_executable(steam_platform_probe steam_platform_probe.cpp)
    target_compile_features(steam_platform_probe PRIVATE cxx_std_17)
    target_link_libraries(steam_platform_probe PRIVATE steam_platform steam_platform_steam_api)
    target_include_directories(steam_platform_probe PRIVATE "${_steam_platform_src}")
    set_target_properties(steam_platform_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/probe")
    nt_set_warning_flags(steam_platform_probe)
    steam_platform_deploy(steam_platform_probe APP_ID 480)
    add_test(NAME steam_platform_probe COMMAND steam_platform_probe)
    set_tests_properties(steam_platform_probe PROPERTIES LABELS "slow" SKIP_RETURN_CODE 77 TIMEOUT 90)
endif()

if(TARGET steam_net_link)
    add_executable(test_steam_net_link test_steam_net_link.c)
    target_link_libraries(test_steam_net_link PRIVATE steam_net_link unity)
    target_include_directories(test_steam_net_link PRIVATE "${_steam_platform_src}")
    if(NOT WIN32)
        target_link_libraries(test_steam_net_link PRIVATE m)
    endif()
    nt_set_warning_flags(test_steam_net_link)
    nt_set_sanitizer_flags(test_steam_net_link)
    steam_platform_deploy(test_steam_net_link)
    add_test(NAME test_steam_net_link COMMAND test_steam_net_link)
    set_tests_properties(test_steam_net_link PROPERTIES LABELS "core")
endif()

# Live runs on the developer's Steam client as Spacewar, each in a folder of its own with
# steam_appid.txt; exit 77 is a skip.
function(_steam_net_link_live name)
    add_executable(${name} ${ARGN})
    target_compile_features(${name} PRIVATE cxx_std_17)
    target_link_libraries(${name} PRIVATE steam_net_link steam_platform_steam_api unity)
    target_include_directories(${name} PRIVATE "${_steam_platform_src}")
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${name}")
    nt_set_warning_flags(${name})
    steam_platform_deploy(${name} APP_ID 480)
endfunction()

if(TARGET steam_net_link AND STEAM_PLATFORM_HAS_SDK AND TARGET net_link_contract)
    _steam_net_link_live(steam_net_link_contract steam_net_link_contract.cpp)
    target_link_libraries(steam_net_link_contract PRIVATE net_link_contract)
    add_test(NAME steam_net_link_contract COMMAND steam_net_link_contract)
    set_tests_properties(steam_net_link_contract PROPERTIES LABELS "slow" SKIP_RETURN_CODE 77 TIMEOUT 400)
endif()

if(TARGET steam_net_link AND STEAM_PLATFORM_HAS_SDK)
    _steam_net_link_live(steam_net_link_probe steam_net_link_probe.cpp)
    add_test(NAME steam_net_link_probe COMMAND steam_net_link_probe)
    set_tests_properties(steam_net_link_probe PROPERTIES LABELS "slow" SKIP_RETURN_CODE 77 TIMEOUT 300)
    _steam_net_link_live(steam_net_link_closes_live steam_net_link_closes_live.cpp)
    add_test(NAME steam_net_link_closes_live COMMAND steam_net_link_closes_live)
    set_tests_properties(steam_net_link_closes_live PROPERTIES LABELS "slow" SKIP_RETURN_CODE 77 TIMEOUT 300)
    # Starts itself again as the guest processes.
    _steam_net_link_live(steam_net_link_two_windows steam_net_link_two_windows.cpp)
    add_test(NAME steam_net_link_two_windows COMMAND steam_net_link_two_windows)
    set_tests_properties(steam_net_link_two_windows PROPERTIES LABELS "slow" SKIP_RETURN_CODE 77 TIMEOUT 240)
endif()

# Every live run, on its simulated no-client path: exit 77 (a skip) as a rule, exit 1 when
# STEAM_PLATFORM_REQUIRE_CLIENT says the run must have had Steam. No Steam is touched.
if(STEAM_PLATFORM_HAS_SDK)
    set(_steam_live_runs "$<TARGET_FILE:steam_platform_probe>")
    foreach(_run IN ITEMS steam_net_link_contract steam_net_link_probe steam_net_link_closes_live steam_net_link_two_windows)
        if(TARGET ${_run})
            string(APPEND _steam_live_runs "|$<TARGET_FILE:${_run}>")
        endif()
    endforeach()
    set(_expect "${CMAKE_CURRENT_LIST_DIR}/expect_no_client_exit.cmake")
    add_test(NAME steam_live_no_client_skips
        COMMAND "${CMAKE_COMMAND}" -E env --unset=STEAM_PLATFORM_REQUIRE_CLIENT
                "${CMAKE_COMMAND}" "-DRUNS=${_steam_live_runs}" -DEXPECT=77 -P "${_expect}")
    add_test(NAME steam_live_no_client_fails_when_required
        COMMAND "${CMAKE_COMMAND}" -E env STEAM_PLATFORM_REQUIRE_CLIENT=1
                "${CMAKE_COMMAND}" "-DRUNS=${_steam_live_runs}" -DEXPECT=1 -P "${_expect}")
    # They run the live executables, so they are slow like them, though they never touch Steam.
    set_tests_properties(steam_live_no_client_skips steam_live_no_client_fails_when_required PROPERTIES LABELS "slow")
endif()

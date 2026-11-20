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

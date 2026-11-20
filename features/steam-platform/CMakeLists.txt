cmake_minimum_required(VERSION 3.25)

# The SDK never enters git: each developer points the build at an unpacked Steamworks SDK
# (the folder that holds public/ and redistributable_bin/). Empty means a build without it.
set(STEAMWORKS_SDK_DIR "$ENV{STEAMWORKS_SDK_DIR}" CACHE PATH "Steamworks SDK root (the sdk folder); empty builds without Steam")

set(STEAM_PLATFORM_HAS_SDK OFF)
if(STEAMWORKS_SDK_DIR)
    set(_sdk "${STEAMWORKS_SDK_DIR}")
    if(NOT EXISTS "${_sdk}/public/steam/steam_api.h")
        message(FATAL_ERROR "STEAMWORKS_SDK_DIR='${_sdk}' is not a Steamworks SDK: public/steam/steam_api.h is missing. Point it at the 'sdk' folder of the unpacked SDK, or clear it to build without Steam.")
    endif()
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_runtime "${_sdk}/redistributable_bin/win64/steam_api64.dll")
        set(_implib "${_sdk}/redistributable_bin/win64/steam_api64.lib")
    elseif(WIN32)
        set(_runtime "${_sdk}/redistributable_bin/steam_api.dll")
        set(_implib "${_sdk}/redistributable_bin/steam_api.lib")
    elseif(APPLE)
        set(_runtime "${_sdk}/redistributable_bin/osx/libsteam_api.dylib")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(_runtime "${_sdk}/redistributable_bin/linuxarm64/libsteam_api.so")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_runtime "${_sdk}/redistributable_bin/linux64/libsteam_api.so")
    else()
        set(_runtime "${_sdk}/redistributable_bin/linux32/libsteam_api.so")
    endif()
    foreach(_file IN ITEMS "${_runtime}" "${_implib}")
        if(_file AND NOT EXISTS "${_file}")
            message(FATAL_ERROR "STEAMWORKS_SDK_DIR='${_sdk}': ${_file} is missing for this platform.")
        endif()
    endforeach()

    # GLOBAL so steam_platform_deploy and the feature's tests can name it from any directory.
    add_library(steam_platform_steam_api SHARED IMPORTED GLOBAL)
    set_target_properties(steam_platform_steam_api PROPERTIES IMPORTED_LOCATION "${_runtime}")
    if(_implib)
        set_target_properties(steam_platform_steam_api PROPERTIES IMPORTED_IMPLIB "${_implib}")
    endif()
    target_include_directories(steam_platform_steam_api SYSTEM INTERFACE "${_sdk}/public")
    get_filename_component(_runtime_name "${_runtime}" NAME)
    if(WIN32)
        # Delay-loaded, so a missing DLL is a FAILED start instead of a loader dialog before
        # main. Interface properties carry both to the final executable through the static lib.
        set_property(TARGET steam_platform_steam_api APPEND PROPERTY
            INTERFACE_LINK_OPTIONS "LINKER:/DELAYLOAD:${_runtime_name}")
        set_property(TARGET steam_platform_steam_api APPEND PROPERTY INTERFACE_LINK_LIBRARIES delayimp)
    endif()
    set(STEAM_PLATFORM_HAS_SDK ON)
endif()
set(STEAM_PLATFORM_HAS_SDK ${STEAM_PLATFORM_HAS_SDK} CACHE INTERNAL "steam-platform builds against the Steamworks SDK")

if(STEAM_PLATFORM_HAS_SDK)
    # CMake links a C++ static library only where C++ is enabled in the top directory, so the
    # game enables it there; enabling it here would leave the game's link without a rule.
    if(NOT CMAKE_CXX_COMPILER_LOADED)
        message(FATAL_ERROR "steam-platform with STEAMWORKS_SDK_DIR has a C++ part: call enable_language(CXX) (or project(... LANGUAGES C CXX)) in the top-level CMakeLists.txt before adding this feature.")
    endif()
    add_library(steam_platform STATIC src/steam_platform.cpp src/steam_platform_common.c)
    target_compile_features(steam_platform PRIVATE cxx_std_17)
    target_compile_definitions(steam_platform PRIVATE STEAM_PLATFORM_RUNTIME_NAME="${_runtime_name}")
    # The SDK's headers stay private: a game includes only the C header.
    target_link_libraries(steam_platform PRIVATE steam_platform_steam_api)
    target_compile_definitions(steam_platform PUBLIC STEAM_PLATFORM_HAS_SDK=1)
else()
    add_library(steam_platform STATIC src/steam_platform_stub.c src/steam_platform_common.c)
endif()
target_include_directories(steam_platform PUBLIC include)
target_compile_features(steam_platform PUBLIC c_std_11)
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(steam_platform)
elseif(NOT MSVC)
    target_compile_options(steam_platform PRIVATE -Wall -Wextra -Werror)
endif()
if(COMMAND nt_set_sanitizer_flags)
    nt_set_sanitizer_flags(steam_platform)
endif()

# The second target, a net-link transport, exists where the consumer added features/net-link
# before this feature; a game without it links steam_platform alone.
if(TARGET net_link)
    if(STEAM_PLATFORM_HAS_SDK)
        add_library(steam_net_link STATIC src/steam_net_link.cpp src/steam_net_link_module.cpp src/steam_net_link_closing.cpp src/steam_net_link_common.c src/steam_net_link_closes.c)
        target_compile_features(steam_net_link PRIVATE cxx_std_17)
        target_link_libraries(steam_net_link PRIVATE steam_platform_steam_api)
    else()
        add_library(steam_net_link STATIC src/steam_net_link_stub.c src/steam_net_link_common.c src/steam_net_link_closes.c)
    endif()
    target_link_libraries(steam_net_link PUBLIC steam_platform net_link)
    target_include_directories(steam_net_link PRIVATE src)
    if(NOT WIN32)
        target_link_libraries(steam_net_link PRIVATE m)
    endif()
    if(COMMAND nt_set_warning_flags)
        nt_set_warning_flags(steam_net_link)
    elseif(NOT MSVC)
        target_compile_options(steam_net_link PRIVATE -Wall -Wextra -Werror)
    endif()
    if(COMMAND nt_set_sanitizer_flags)
        nt_set_sanitizer_flags(steam_net_link)
    endif()
else()
    message(STATUS "steam-platform: steam_net_link is not built: the target net_link does not exist yet (add features/net-link before this feature)")
endif()

# steam_platform_deploy(<target> [APP_ID <id>])
# Copies Steam's runtime library beside the target's executable after each build and lets the
# executable find it there. APP_ID also writes steam_appid.txt there, which lets a development
# build run outside Steam; without APP_ID a stale one is removed, because a release folder
# with that file no longer restarts through Steam. A development and a release target must not
# share an output folder. Without the SDK it does nothing.
function(steam_platform_deploy target)
    cmake_parse_arguments(D "" "APP_ID" "" ${ARGN})
    if(NOT STEAM_PLATFORM_HAS_SDK)
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:steam_platform_steam_api>" "$<TARGET_FILE_DIR:${target}>"
        VERBATIM)
    if(APPLE)
        set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "@executable_path")
        set_property(TARGET ${target} APPEND PROPERTY INSTALL_RPATH "@executable_path")
    elseif(UNIX)
        set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "$ORIGIN")
        set_property(TARGET ${target} APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
    if(D_APP_ID)
        set(_appid_file "${CMAKE_CURRENT_BINARY_DIR}/steam_platform_deploy/${target}/steam_appid.txt")
        file(CONFIGURE OUTPUT "${_appid_file}" CONTENT "${D_APP_ID}\n")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_appid_file}" "$<TARGET_FILE_DIR:${target}>/steam_appid.txt"
            VERBATIM)
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -f "$<TARGET_FILE_DIR:${target}>/steam_appid.txt"
            VERBATIM)
    endif()
endfunction()

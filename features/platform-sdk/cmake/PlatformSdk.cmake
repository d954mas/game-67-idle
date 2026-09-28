# Closure renames every property of the client's EM_JS code unless an externs
# file names it; the platform bundle is outside Closure, so the names they
# share must be listed. Call for a target that links with --closure 1.
#
#   include("${PLATFORM_SDK_DIR}/cmake/PlatformSdk.cmake")
#   platform_sdk_closure_externs(my_client)

function(platform_sdk_closure_externs target)
    set(_externs "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../web/platform_sdk.externs.js")
    cmake_path(NORMAL_PATH _externs)
    target_link_options(${target} PRIVATE "SHELL:--closure-args=--externs=${_externs}")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${_externs}")
endfunction()

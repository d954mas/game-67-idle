# A small engine window that asks Steam for its overlay. Optional: it needs the SDK, the
# engine's window and graphics targets and a signed-in Steam client, so it is never part of
# the default build or of CTest.
if(NOT STEAM_PLATFORM_HAS_SDK OR NOT TARGET nt_window OR NOT TARGET nt_gfx)
    return()
endif()

add_executable(steam_platform_overlay_example EXCLUDE_FROM_ALL overlay_example.c)
target_link_libraries(steam_platform_overlay_example PRIVATE
    steam_platform nt_core nt_app nt_input nt_window nt_gfx nt_log
    nt_basisu_transcoder_stub nt_meshwire_stub)
nt_set_warning_flags(steam_platform_overlay_example)
steam_platform_deploy(steam_platform_overlay_example APP_ID 480)

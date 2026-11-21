# The stand in an engine window ("guest's view"), on debug shapes. Optional: it needs the
# engine's window, graphics and shape renderer, so it is never part of the default build or of
# CTest. The run, camera and recording (stand_view.c) are kept apart from the drawing
# (stand_view_draw.c), so a game's target links body_sync_view and passes its own drawer.
if(NOT TARGET nt_window OR NOT TARGET nt_gfx OR NOT TARGET nt_shape_renderer OR NOT TARGET nt_app)
    message(STATUS "body-sync: the stand viewer needs the engine's window and renderer; skipped")
    return()
endif()

add_library(body_sync_view STATIC EXCLUDE_FROM_ALL stand_view.c stand_view_draw.c)
target_include_directories(body_sync_view PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
# nt_gfx is public: a drawer's configure_gfx edits its description. The engine's codec stubs
# are the program's choice, not the library's: a game links its real transcoder and mesh codec.
target_link_libraries(body_sync_view PUBLIC body_sync_stand nt_gfx
    PRIVATE nt_core nt_app nt_time nt_input nt_window nt_shape_renderer nt_math nt_log)
# Recording and screenshots need the engine's recorder, which only some platforms build.
if(TARGET nt_recorder AND TARGET nt_fpng)
    target_link_libraries(body_sync_view PRIVATE nt_recorder nt_fpng)
    target_compile_definitions(body_sync_view PRIVATE BODY_SYNC_VIEW_RECORDER=1)
else()
    message(STATUS "body-sync: no engine recorder on this platform; the viewer cannot record")
endif()
nt_set_warning_flags(body_sync_view)

add_executable(body_sync_stand_view EXCLUDE_FROM_ALL main.c)
target_link_libraries(body_sync_stand_view PRIVATE body_sync_view nt_basisu_transcoder_stub nt_meshwire_stub)
nt_set_warning_flags(body_sync_stand_view)

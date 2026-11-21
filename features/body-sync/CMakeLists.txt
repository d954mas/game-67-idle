cmake_minimum_required(VERSION 3.25)

# The consumer adds net-link, net-bits and coop-net first; Box3D comes through box3d_enable.
include("${CMAKE_CURRENT_LIST_DIR}/../box3d/cmake/Box3D.cmake")

function(body_sync_flags target)
    target_compile_features(${target} PUBLIC c_std_17)
    if(COMMAND nt_set_warning_flags)
        nt_set_warning_flags(${target})
    elseif(NOT MSVC)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
    if(COMMAND nt_set_sanitizer_flags)
        nt_set_sanitizer_flags(${target})
    endif()
    if(UNIX)
        target_link_libraries(${target} PRIVATE m)
    endif()
endfunction()

# The sync seam a game links. Its header names b3BodyId, so Box3D's include goes with it.
add_library(body_sync STATIC src/body_sync_stub.c src/stub_wire.c)
target_include_directories(body_sync PUBLIC include)
box3d_enable(body_sync)
target_link_libraries(body_sync PUBLIC nt::box3d)
body_sync_flags(body_sync)

# The stand's decisions without physics: scenes as data, the catalog, the scenario parser, the
# schedule, the host rules, metrics and JSON. No Box3D, so its tests run alone.
add_library(body_sync_stand_logic STATIC
    src/stand_scene.c
    src/stand_catalog.c
    src/stand_scenario.c
    src/stand_logic.c
    src/stand_metrics.c)
target_include_directories(body_sync_stand_logic PUBLIC include)
target_link_libraries(body_sync_stand_logic PUBLIC net_link)
body_sync_flags(body_sync_stand_logic)

# The stand: worlds, the test character, the runner over net_link_sim and coop-net, the CLI.
add_library(body_sync_stand STATIC src/stand_world.c src/stand_runner.c src/stand_main.c)
target_link_libraries(body_sync_stand PUBLIC body_sync_stand_logic PRIVATE body_sync coop_net net_link)
box3d_enable(body_sync_stand)
body_sync_flags(body_sync_stand)

add_executable(body_sync_stand_cli cli/main.c)
set_target_properties(body_sync_stand_cli PROPERTIES OUTPUT_NAME body_sync_stand)
target_link_libraries(body_sync_stand_cli PRIVATE body_sync_stand)
body_sync_flags(body_sync_stand_cli)

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

# The parts that decide without touching a body: the codec, the sender, the naive receiver,
# authority and the stub's record bytes. No Box3D, so their tests run alone.
add_library(body_sync_parts STATIC src/sync_codec.c src/sync_sender.c src/sync_receiver.c src/sync_playback.c src/sync_authority.c src/stub_wire.c)
target_include_directories(body_sync_parts PUBLIC include src)
target_link_libraries(body_sync_parts PUBLIC net_bits)
body_sync_flags(body_sync_parts)

# The two implementations of body_sync.h; a program links one. body_sync is the real seam a
# game links; body_sync_stub is the naive baseline the stand measures it against. The header
# names b3BodyId, so Box3D's include goes with either.
foreach(impl body_sync body_sync_stub)
    if(impl STREQUAL "body_sync")
        add_library(${impl} STATIC src/body_sync_real.c)
    else()
        add_library(${impl} STATIC src/body_sync_stub.c)
    endif()
    target_include_directories(${impl} PUBLIC include)
    box3d_enable(${impl})
    target_link_libraries(${impl} PUBLIC nt::box3d PRIVATE body_sync_parts)
    body_sync_flags(${impl})
endforeach()

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

# The stand: worlds, the test character, the runner over net_link_sim and coop-net, the CLI,
# once on each implementation so one scenario and seed compare the two.
foreach(impl body_sync body_sync_stub)
    if(impl STREQUAL "body_sync")
        set(stand body_sync_stand)
        set(cli body_sync_stand_cli)
    else()
        set(stand body_sync_stand_stub)
        set(cli body_sync_stand_cli_stub)
    endif()
    add_library(${stand} STATIC src/stand_world.c src/stand_runner.c src/stand_main.c)
    target_link_libraries(${stand} PUBLIC body_sync_stand_logic ${impl} PRIVATE coop_net net_link)
    box3d_enable(${stand})
    body_sync_flags(${stand})
    add_executable(${cli} cli/main.c)
    set_target_properties(${cli} PROPERTIES OUTPUT_NAME ${stand})
    target_link_libraries(${cli} PRIVATE ${stand})
    body_sync_flags(${cli})
endforeach()

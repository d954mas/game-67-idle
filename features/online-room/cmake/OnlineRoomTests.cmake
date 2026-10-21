# Call after game_add_c_test exists and the online-room directory was added.
function(online_room_add_tests)
    foreach(_name IN ITEMS ticket meta meta_url entry_ladder rtt meta_entry meta_account perf schedule)
        game_add_c_test(online_room_${_name} WARNINGS TIER core
            SOURCES "${ONLINE_ROOM_DIR}/tests/online_room_${_name}.c"
            LIBS online_room)
    endforeach()
    # The perf window is read against the shape the metaserver pins too.
    target_compile_definitions(online_room_perf PRIVATE
        ONLINE_ROOM_PERF_FIXTURE="${ONLINE_ROOM_DIR}/../metaserver/testdata/room_perf_v1.json")
endfunction()

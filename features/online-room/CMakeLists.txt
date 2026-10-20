# The room's and the client's half of the metaserver contract (CONTRACT.md).
# The consumer sets the sizes before add_subdirectory. They shape structs, so
# they are PUBLIC definitions with no defaults: every unit that includes a
# sized header sees the values the library was built with.
foreach(_size IN ITEMS ONLINE_ROOM_PLAYERS_MAX ONLINE_ROOM_PARTING_MAX ONLINE_ROOM_PROFILE_TEXT_MAX)
    if(NOT "${${_size}}" MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "online-room: set ${_size} to a positive number before add_subdirectory")
    endif()
endforeach()

add_library(online_room STATIC
    src/ticket.c src/meta.c src/memory_store.c
    src/entry.c src/rtt.c src/meta_entry.c src/meta_account.c)
target_include_directories(online_room PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(online_room PUBLIC
    ONLINE_ROOM_PLAYERS_MAX=${ONLINE_ROOM_PLAYERS_MAX}U
    ONLINE_ROOM_PARTING_MAX=${ONLINE_ROOM_PARTING_MAX}U
    ONLINE_ROOM_PROFILE_TEXT_MAX=${ONLINE_ROOM_PROFILE_TEXT_MAX}U)
target_link_libraries(online_room PUBLIC nt::network_core_codec cjson)
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(online_room)
endif()

# The transports over the engine's HTTP client. The consumer compiles them into
# its own target, beside the nt_http build it sizes (the room's must hold
# ONLINE_META_TRANSPORT_SLOTS requests).
set(ONLINE_ROOM_DIR "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
set(ONLINE_ROOM_META_NT_HTTP_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/meta_nt_http.c" PARENT_SCOPE)
set(ONLINE_ROOM_META_ENTRY_NT_HTTP_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/meta_entry_nt_http.c" PARENT_SCOPE)

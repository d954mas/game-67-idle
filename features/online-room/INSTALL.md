# Online Room Install

## Build

After `features/network-core` and a `cjson` target exist:

```cmake
set(ONLINE_ROOM_PLAYERS_MAX ${SEATS})            # seats per room
math(EXPR ONLINE_ROOM_PARTING_MAX "2 * ${SEATS}") # last saves a room keeps out
set(ONLINE_ROOM_PROFILE_TEXT_MAX 8192)           # bytes of one profile document
add_subdirectory("${GAME_REPO_ROOT}/features/online-room" "${CMAKE_BINARY_DIR}/_online_room")

target_link_libraries(game_layer PUBLIC online_room)
# The room host, beside the nt_http build sized for ONLINE_META_TRANSPORT_SLOTS:
target_sources(room PRIVATE "${ONLINE_ROOM_META_NT_HTTP_SOURCE}")
# The client and any headless client:
target_sources(client PRIVATE "${ONLINE_ROOM_META_ENTRY_NT_HTTP_SOURCE}")
```

The room image's build context must carry `features/online-room/`.

## Verify

From the consumer's tests CMake, once `game_add_c_test` exists:

```cmake
include("${ONLINE_ROOM_DIR}/cmake/OnlineRoomTests.cmake")
online_room_add_tests()
```

The tests run as `online_room_*`, tier `core`.

## Uninstall

Remove the link, the transport sources, the test include and the
`add_subdirectory`, and the folder from the room image's context.

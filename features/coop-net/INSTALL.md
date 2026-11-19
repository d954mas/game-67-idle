# Install coop-net

1. Add the dependency to the game's `dependencies.json`:

   ```json
   {
     "id": "coop-net",
     "source": "features/coop-net",
     "version": "0.1.0",
     "compatibility": "loopback room with two clients; game tests green"
   }
   ```

2. In the game's `CMakeLists.txt`, native builds only (the room server is native):

   ```cmake
   if(NOT EMSCRIPTEN)
       add_subdirectory("${GAME_REPO_ROOT}/features/network-core" "${CMAKE_BINARY_DIR}/_network_core")
       add_subdirectory("${GAME_REPO_ROOT}/features/coop-net" "${CMAKE_BINARY_DIR}/_coop_net")
       target_link_libraries(<game-target> PRIVATE coop_net)
   endif()
   ```

   `network-core` needs libcurl with WebSockets on native (`NT_HTTP_WEBSOCKETS=ON`).

3. Implement the callbacks: `on_tick` steps the game's world, `write_snapshot` writes the game's
   payload, `on_input` decodes one seat's payload. The client needs `on_snapshot` and, for
   prediction, the acknowledgement of the seat inside `acks`.

4. Verify: run a room and two clients on loopback, then the game's own test suite.

## Uninstall

Delete the `dependencies.json` entry, remove the `add_subdirectory` and the link line, and remove
the game's callbacks. Nothing else in the game refers to the feature.

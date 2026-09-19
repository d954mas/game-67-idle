# Install network-core

1. Resolve `GAME_REPO_ROOT` with the existing game scaffold, then add the
   feature once, after the engine (its helpers `nt_set_warning_flags` and
   `nt_set_sanitizer_flags` are picked up when they exist):

   ```cmake
   add_subdirectory("${GAME_REPO_ROOT}/features/network-core" "${CMAKE_BINARY_DIR}/_network_core")
   target_link_libraries(room_server PRIVATE nt::network_core)   # native room
   target_link_libraries(game PRIVATE nt::network_core)          # native or web client
   ```

   Link `nt::network_core_codec` alone where only the reader/writer is needed
   (a wire-contract library shared by client and room). On web the target
   adds `-lwebsocket.js` to the link automatically.

2. Add to the consuming game's `dependencies.json` features array:

   ```json
   {"id":"network-core","source":"features/network-core","version":"0.1.0","compatibility":"..."}
   ```

3. Write the game's messages on top of the codec. Message type `0` is HELLO
   and belongs to the feature; game types start at `NET_MSG_APP_FIRST`. Give
   the server config a `protocol_version` and bump it whenever the wire
   contract changes; clients on the old version are closed with
   `NET_CLOSE_VERSION` before any game message is parsed.

4. Size the limits from the contract, not from guesses: `max_message_bytes`
   is the largest legal inbound message, `send_queue_bytes` how far a slow
   peer may fall behind before it is dropped, `max_messages_per_second` a
   multiple of the client's send cadence.

5. Verify from the Studio root:

   ```text
   node --test features/network-core/tests/integration.test.mjs
   ```

   then the game's own `node tools/game.mjs test` and web build.

Uninstall: remove the `add_subdirectory`, the link lines, the dependency row
and the game's message code. Nothing else is installed. Do not remove the
shared feature while another game consumes it.

Distributed binaries must carry `vendor/libwebsockets/LICENSE` (MIT) in their
third-party notices.

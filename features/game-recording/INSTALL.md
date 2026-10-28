# Install

Requires audio-core 1.3.0, an initialized native audio backend and engine recorder
support. Use the current engine APIs; do not copy recorder implementation code.

1. Before adding the engine, set `NT_DEVAPI_GROUP_RECORDER=ON` only when
   `GAME_DEVAPI_ENABLED AND WIN32 AND NOT EMSCRIPTEN`. Force it OFF otherwise,
   including production and web builds. Keep normal DevAPI registration.
2. After creating host and audio targets:

   ```cmake
   include("${GAME_REPO_ROOT}/features/game-recording/recording.cmake")
   game_recording_attach(game game_audio)
   ```

   Omit the second target if audio-core is compiled directly into the host.
   The helper attaches the source, includes, links and native capture defines.
3. Include `features/recording/recording.h`. Call `game_recording_register()`
   after default DevAPI registration. Disabled builds have inline no-op APIs.
4. Reserve five additional graphics buffer slots under `NT_DEVAPI_GROUP_RECORDER`
   before graphics initialization for the engine's asynchronous readbacks.
5. After `nt_gfx_end_frame` and before swapping the window, call
   `game_recording_frame(now, dt)` exactly once per presented frame. `now` is
   wall-clock time in seconds and `dt` the frame advance; the owned audio take
   refreshes its timestamp at presentation after draining the device queue.
6. Call `game_recording_shutdown()` before shutting down audio or graphics.
   The game retains its ordinary audio gesture, pause, settings and lifecycle.
7. Record both feature versions in the game's dependency record. Build the
   native DevAPI host with its `tools/game.mjs` command and verify a real take.

To register the focused native test in a consumer that defines `game_add_c_test`,
include `cmake/GameRecordingTests.cmake` and call `game_recording_add_tests()`.
Run its `test_game_recording` target through the shared game test runner.

To uninstall, remove the host calls/include, CMake helper invocation, added
buffer reserve and dependency record. No assets or saved state are owned here.

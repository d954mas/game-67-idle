# Lap-time integration example

This module shows a second game's leaderboard: the best completed lap in
milliseconds, sorted ascending, all-time only. `leaderboards.json` supplies
the generated board table. Zero means no completed lap in this example, so
`lap_time_example_submit_ms(0)` returns false without submitting a score.
Positive values return true when handed to the facade; that is not a network
acknowledgment. The feature itself permits zero scores for games that use them.

The consumer owns save callbacks, backend selection and UI. This example owns
the one facade instance; do not initialize another leaderboard facade beside it.
It introduces no game assets, scene or portal JavaScript.

## Wiring into a template-derived game

The template already compiles the leaderboard sources. Add this module and
generate its board table after the game executable has been created:

```cmake
set(LAP_EXAMPLE_DIR "${LEADERBOARD_DIR}/example")
target_sources(${GAME_TARGET} PRIVATE "${LAP_EXAMPLE_DIR}/lap_time_example.c")
target_include_directories(${GAME_TARGET} PRIVATE "${LAP_EXAMPLE_DIR}")
include("${LEADERBOARD_DIR}/cmake/LeaderboardManifest.cmake")
leaderboard_generate_boards(
    TARGET ${GAME_TARGET}
    MANIFEST "${LAP_EXAMPLE_DIR}/leaderboards.json"
    PUBLISH_TARGET "${GAME_PUBLISH_TARGET}"
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
```

A real game copies the manifest into its own source directory and points
`MANIFEST` there. Generate only one `game_leaderboards.h` for each consumer.

Pass the game's `leaderboard_host_t` and the backend selected from the
generated `GAME_LEADERBOARD_HAS_PORTAL`, `GAME_LEADERBOARD_HAS_HTTP` and
`GAME_LEADERBOARD_HAS_NONE` flags to `lap_time_example_init`. All boards on one
publish target must use the same family. `portal` uses
`leaderboard_portal_backend()` and requires the platform SDK lifecycle;
`http` uses `leaderboard_http_backend()` with game-owned endpoint, key and
cadence configuration; `none` passes NULL. The backend userdata must remain
alive until shutdown. A test may explicitly inject `leaderboard_mock_backend()`.

The HTTP service is supplied by the consumer. For this manifest its metric
field is `lap_time` and the response is under `lap_time.all`; the descriptive
`metric` label is not the wire field. The service must retain the smaller lap
time and return ascending rows; its bucket statistics count smaller, better
times for place estimation. This example
does not deploy a service or embed credentials. Portal boards must also be
configured for ascending scores. When a portal reports no board capability,
the game hides the launcher; it never falls back to its HTTP service.

Call `lap_time_example_submit_ms(saved_best_ms)` after loading the game's best
completed lap, and on each new completed lap. The game persists that best time
itself; the facade's acknowledged-score keys do not replace game progression.
Call `lap_time_example_update()` once per frame, including while a board modal
is open, and `lap_time_example_shutdown()` at exit.

The screen reads `lap_time_example_snapshot()`. Show its launcher only when
`snapshot.ui.show_launcher` is true. On opening the in-game screen call
`lap_time_example_open()` to request a fresh page; use the same call for manual
retry. Render `snapshot.view` and honor loading, login and retry flags. Login
is a player gesture through the platform SDK. A native-popup-only backend
uses `leaderboard_open_native(leaderboard_board(GAME_LEADERBOARD_LAP_TIME))`
instead of drawing this in-game screen. UI layout and real fonts remain the
game's responsibility.

## Consumer test

The template already registers `test_leaderboard_example` as a core test.
For an older consumer, after its `GameTests.cmake` include, add:

```cmake
if(NOT EMSCRIPTEN)
    include("${LEADERBOARD_DIR}/cmake/LeaderboardExample.cmake")
    leaderboard_add_example_test()
endif()
```

Build and run the consumer through the Studio game runner. The test injects
canned ascending rows and checks the no-result sentinel, better-time
coalescing, explicit screen refresh, returned view and shutdown. It proves
local C integration; it does not prove vendor SDK behavior or visual quality.

The manifest can be checked separately from the Studio root:

```sh
node features/leaderboard/scripts/leaderboards.mjs validate --manifest features/leaderboard/example/leaderboards.json
```

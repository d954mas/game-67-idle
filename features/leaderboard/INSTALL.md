# leaderboard Install

The template already compiles the pack (`LEADERBOARD_SOURCES` in
`templates/template/CMakeLists.txt`) and registers its tests. A game wires it
in four steps; none of them names a portal.

## Install

1. Declare the boards. Until the manifest generator exists, hand the facade a
   table of `leaderboard_board_def_t` (id, sort, declared scopes, the board's
   name at the portal of this build).
2. Implement the two host callbacks against the game's save. The pack stores
   strings under `lb.id`, `lb.day`, `lb.day.<board>` and
   `lb.sent.<board>.<scope>`; `load` answers false for an absent key.
3. Pick the backend for the build target and call `leaderboard_init` once,
   `leaderboard_update` once per frame and `leaderboard_shutdown` at exit:

   ```c
   #include "features/leaderboard/leaderboard.h"
   #include "features/leaderboard/leaderboard_mock.h"

   static leaderboard_mock_t s_mock;

   static const leaderboard_board_def_t k_boards[] = {
       {.id = "score", .sort = LEADERBOARD_SORT_DESC,
        .scopes = (1u << LEADERBOARD_SCOPE_ALL_TIME) | (1u << LEADERBOARD_SCOPE_UTC_DAY)},
   };

   leaderboard_mock_defaults(&s_mock);
   const leaderboard_config_t config = {
       .host = {.load = my_save_load, .store = my_save_store},
       .backend = leaderboard_mock_backend(),
       .backend_userdata = &s_mock,
       .boards = k_boards,
       .board_count = 1,
   };
   leaderboard_init(&config);
   ```

4. Call `leaderboard_submit(board, scope, value, extra)` where the metric
   changes, and draw the screen from `leaderboard_view_get` while obeying
   `leaderboard_ui_state`. Pack game-specific row data with
   `leaderboard_extra_set` and unpack it with `leaderboard_extra_get`.

## Board Manifest

The game declares its boards in `leaderboards.json` next to its `CMakeLists.txt`
and the build turns that into the table `leaderboard_init` receives. A board id
is written once; the console names, the Playgama config block and the C
constants all come from it.

```json
{
  "schema": "ai_studio.leaderboards.v1",
  "boards": [
    {
      "id": "score",
      "metric": "points",
      "sort": "desc",
      "scopes": ["all_time", "utc_day"],
      "portal_ids": { "yandex": "score", "playgama": { "id": "score", "isMain": true } },
      "backends": {
        "yandex": "portal", "crazygames": "none", "playgama": "portal",
        "poki": "http", "itch": "http", "local": "http"
      }
    }
  ]
}
```

`backends` names a family per publish target, never a capability: `portal` means
"ask the portal at run time", so a Playgama build whose host platform has no
board simply reports none. `sort` decides which of two submissions wins when
they coalesce — `asc` for lap times. A scope no target can serve is refused when
the manifest is validated rather than dropped silently at run time.

Wire the generator in the game's `CMakeLists.txt`:

```cmake
include("${LEADERBOARD_DIR}/cmake/LeaderboardManifest.cmake")
leaderboard_generate_boards(
    TARGET         ${GAME_TARGET}
    MANIFEST       "${CMAKE_CURRENT_SOURCE_DIR}/leaderboards.json"
    PUBLISH_TARGET "${GAME_PUBLISH_TARGET}"
    OUTPUT_DIR     "${CMAKE_CURRENT_BINARY_DIR}/generated")
```

The target then includes `game_leaderboards.h`, which defines
`GAME_LEADERBOARD_BOARDS`, `GAME_LEADERBOARD_BOARD_COUNT` and one id constant
per board. The header is a build product: switching publish target regenerates
it with that portal's board ids.

Neither Yandex nor CrazyGames can create a board over an API, so print the ones
a human has to type into a console:

```
node features/leaderboard/scripts/leaderboards.mjs checklist --manifest <game>/leaderboards.json
node features/leaderboard/scripts/leaderboards.mjs validate  --manifest <game>/leaderboards.json
node features/leaderboard/scripts/leaderboards.mjs playgama-config --manifest <game>/leaderboards.json --config <game>/web/playgama-bridge-config.json
```

The last one rewrites only the `leaderboards` block; every other value in that
hand-authored file is left as it is.

## HTTP Backend

For portals without a board of their own (and local builds). The game owns
the endpoint, the obfuscation key and the poll cadence; the pack owns the
protocol. Fill a `leaderboard_http_t`, hand it to the facade as the backend
userdata, and keep it alive until `leaderboard_shutdown`:

```c
#include "features/leaderboard/leaderboard_http.h"

static leaderboard_http_t s_http;

s_http.config = (leaderboard_http_config_t){
    .url = MY_BOARD_URL,          /* "" keeps the backend dormant: no board, no requests */
    .key = MY_BOARD_XOR_KEY,      /* mirrored by the server function */
    .repeat_delay_s = 900.0,      /* healthy poll */
    .error_delay_s = 10.0,        /* fast retry ... */
    .error_fast_tries = 5,        /* ... this many times, then back to repeat_delay_s */
    .transport = NULL,            /* the engine's nt_http and clocks */
};
const leaderboard_config_t config = {
    .host = {.load = my_save_load, .store = my_save_store},
    .backend = leaderboard_http_backend(),
    .backend_userdata = &s_http,
    .boards = k_boards,
    .board_count = 1,
};
```

What the backend needs from the rest of the wiring:

- `lb.id` in the host callbacks must answer the anonymous id the server rows
  are keyed by. A game that already stores one maps the key onto that field;
  the facade mints a uuid v4 only when `load` answers nothing.
- The board's `portal_id` (its `id` when none) is the metric's name on the
  wire: the request carries `<name>` and `<name>_day`, the response is read
  under `<name>`. Set it to whatever the server function calls the metric.
- Every request is also the submit: the body carries the facade's current
  value for every declared scope, so the server row is never overwritten with
  a smaller number. Call `leaderboard_submit` with the game's counters at
  startup as well as on every change.
- Row data beyond the value rides in `extra`: each `k=v;` pair becomes a body
  field (a plain integer as a JSON number, anything else as a string), and
  every server row field beyond `user_id` and `value` comes back packed the
  same way. Pairs named `user_id`, `day`, `<name>` or `<name>_day` are dropped.
- Both scopes are served; the manifest's `scopes` mask hides one.

Dev builds point `url` at a local mock such as the seeding game's
`tools/lb_mock_server.mjs` (any server that speaks the same shape works):

```
node tools/lb_mock_server.mjs            # serves http://127.0.0.1:8787/leaderboards
cmake -DLEADERBOARD_URL=http://127.0.0.1:8787/leaderboards ...
```

with the game's config header taking the URL from that define, and the mock's
key matching `.key`.

## Portal Backend

For a build whose target has a portal board (Yandex, CrazyGames, Playgama),
hand `leaderboard_init` the portal backend and give each board its portal id:

```c
#include "features/leaderboard/leaderboard_portal.h"

static const leaderboard_board_def_t k_boards[] = {
    {.id = "score", .sort = LEADERBOARD_SORT_DESC,
     .scopes = (1u << LEADERBOARD_SCOPE_ALL_TIME), .portal_id = "planets"},
};

const leaderboard_config_t config = {
    .host = {.load = my_save_load, .store = my_save_store},
    .backend = leaderboard_portal_backend(),
    .boards = k_boards,
    .board_count = 1,
};
```

The backend takes no userdata. `platform_sdk_init()` may still be pending
when `leaderboard_init` runs: the board answers no capability until the
portal is up, and a score submitted meanwhile is sent on the first
`leaderboard_update()` after it is.

Before any call answers, the board must exist at the portal:

- Yandex: create the leaderboard in the developer console; `portal_id` is its
  technical name. `getEntries` answers 404 until then.
- CrazyGames: the leaderboard is set up on the game's console page and the
  encryption key it issues reaches the adapter as
  `__PLATFORM_SDK_CONFIG__.leaderboardKey`; without it nothing is written.
  `portal_id` is any non-empty string: the portal has one board per game.
- Playgama: declare the board in the `leaderboards` block of the game's
  `playgama-bridge-config.json`; `portal_id` is that block's `id`. The host
  platform decides at run time what the board can do; `not_available` is a
  correct answer.

Poki, itch and local builds keep the HTTP backend (or the mock).

## Verify

```
cmake --build templates/template/build/native-debug --target test_leaderboard test_leaderboard_core test_leaderboard_http
ctest --test-dir templates/template/build/native-debug -R leaderboard --output-on-failure
```

## Uninstall

Remove `${LEADERBOARD_SOURCES}` and `${LEADERBOARD_INC}` from the game target,
the two `game_add_c_test` calls, and the game's `leaderboard_*` calls. The
host keys left in old saves are harmless.

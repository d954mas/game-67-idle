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

Not yet written.

## HTTP Backend

Not yet written.

## Portal Backend

Not yet written.

## Verify

```
cmake --build templates/template/build/native-debug --target test_leaderboard test_leaderboard_core
ctest --test-dir templates/template/build/native-debug -R leaderboard --output-on-failure
```

## Uninstall

Remove `${LEADERBOARD_SOURCES}` and `${LEADERBOARD_INC}` from the game target,
the two `game_add_c_test` calls, and the game's `leaderboard_*` calls. The
host keys left in old saves are harmless.

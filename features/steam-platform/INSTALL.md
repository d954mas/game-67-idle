# Install steam platform

## Get the SDK

Download the Steamworks SDK from partner.steamgames.com, Downloads (it needs a
Steamworks login), version 1.58 or newer, and unpack it outside every
repository. Pass its `sdk` folder to a build tree of its own with
`-DSTEAMWORKS_SDK_DIR=<unpacked>/sdk`. Do not set the environment variable of
that name globally: every fresh tree, the standard one too, would then build
the SDK variant, and the stub would no longer be exercised. Never copy SDK
headers, import libraries or the archive into a repository.

## Link

The feature builds in any C project. With the SDK it adds C++ files, and
CMake links a C++ static library only where C++ is enabled in the top-level
directory, so a game that builds with the SDK enables it there:

```cmake
project(game LANGUAGES C)
if(STEAMWORKS_SDK_DIR)
    enable_language(CXX)
endif()

set(STEAM_PLATFORM_DIR "${GAME_REPO_ROOT}/features/steam-platform")
add_subdirectory("${STEAM_PLATFORM_DIR}" "${CMAKE_BINARY_DIR}/steam-platform")
target_link_libraries(game PRIVATE steam_platform)

# Development: steam_api runtime beside the exe and steam_appid.txt naming Spacewar.
steam_platform_deploy(game APP_ID 480)
# Release: the runtime only; a stale steam_appid.txt is removed after each build.
# steam_platform_deploy(game)
```

For the Steam transport, add `features/net-link` before this feature; the
target `steam_net_link` exists only then, and a game without net-link links
`steam_platform` alone:

```cmake
add_subdirectory("${GAME_REPO_ROOT}/features/net-link" "${CMAKE_BINARY_DIR}/net-link")
add_subdirectory("${STEAM_PLATFORM_DIR}" "${CMAKE_BINARY_DIR}/steam-platform")
target_link_libraries(game PRIVATE steam_net_link) # brings steam_platform and net_link
```

Leave `STEAMWORKS_SDK_DIR` empty for a build without Steam; nothing else
changes in the game. A consumer may report which build it is:
`STEAM_PLATFORM_HAS_SDK` is defined for everything that links `steam_platform`
when the SDK is present.

`steam_platform_deploy(<target> [APP_ID <id>])` runs each time the target is
linked:

- it copies the SDK's `redistributable_bin` runtime library next to the
  executable; the game needs it there (on Windows `start` answers `FAILED`
  naming the file when it is missing; on Linux and macOS the system loader
  refuses to start the program, and the function sets an `$ORIGIN` /
  `@executable_path` run path; macOS has not been built);
- with `APP_ID` it writes `steam_appid.txt` there, for development only;
- without `APP_ID` it removes a `steam_appid.txt` left there by an earlier
  development build, because a release folder with that file never restarts
  through Steam.

A development target and a release target must not build into the same
folder. Without the SDK the function does nothing. Record the feature source
and version from `feature.json` in the consuming game's `dependencies.json`.

## Use

1. Start before the window and its graphics context exist, so the overlay can
   hook them:

   ```c
   char why[256];
   steam_platform_config_t steam = {.app_id = GAME_STEAM_APP_ID,
                                    .restart_through_steam = GAME_RELEASE};
   switch (steam_platform_start(&steam, why, sizeof why)) {
   case STEAM_PLATFORM_READY: break;
   case STEAM_PLATFORM_RESTARTING: return 0; /* exit now, open no window */
   default: log_info("solo: %s", why); break; /* NO_SDK, NO_CLIENT, FAILED */
   }
   ```

   Development builds use `app_id = 480` with `restart_through_steam = false`;
   Steam finds `steam_appid.txt` beside the executable. With
   `restart_through_steam` a build started outside Steam gets `RESTARTING`,
   by Steam's documentation also on a machine where Steam is not running.
2. Each frame, after the window has presented: `steam_platform_run_frame()`,
   then `steam_platform_next_event` until it returns false. On `OVERLAY` with
   `active`, pause what must not run under the overlay; resume when it closes.
3. Read `steam_platform_user()` when an identity is needed; zero means solo.
4. `steam_platform_open_overlay("friends")` (or another dialog) from a menu
   button; check `steam_platform_overlay_ready()` before showing such a button.
5. Network, when the game links `steam_net_link`: after a `READY` start open
   a link, then call `steam_platform_run_frame()` before `net_link_poll` each
   frame and `net_link_flush` at the end of the tick:

   ```c
   steam_net_link_config_t cfg = {.lanes = coop_session_lanes(), .max_peers = 3};
   net_link_t *link = hosting ? steam_net_link_host_ip(&cfg, NULL, 27777)         /* LAN */
                              : steam_net_link_join_ip(&cfg, "192.168.1.20", 27777);
   /* Two windows on one PC: host on "127.0.0.1", join "127.0.0.1". */
   ```

   Every open answers NULL without the SDK or without Steam, so the game falls
   back to solo or to another transport with the same code. A NULL `address`
   listens on every interface, where a system firewall may ask about the
   executable; `"127.0.0.1"` listens on the loopback only, which firewalls do
   not filter, and is what the tests use.
6. Shutdown, in this order: destroy every link; run frames while
   `steam_net_link_lingering()`; then `steam_platform_stop()`; then the window.

   ```c
   net_link_destroy(link);
   while (steam_net_link_lingering()) { /* false by itself within 8 s */
       steam_platform_run_frame();
       sleep_ms(10);
   }
   steam_platform_stop();
   ```

   A stop cuts every connection at once. A game that stops right after
   `net_link_close` and `net_link_destroy` loses everything that close had
   queued and its reason: the other side reads REMOTE_CLOSE and none of the
   messages. A host that just closed a peer may be destroyed at once; its port
   stays with the module until the close has left, and the loop covers it.

   The loop is what a full quit costs: about half the link's `timeout_s` after
   a clean close, 3 s at most, and `close_wait_s` more on a path that would not
   acknowledge (8 s in all by default). Set `close_wait_s` to what a quit may
   cost; past it the other side still reads the reason and loses the rest. It
   runs only where the process ends; leaving a session for the menu destroys
   the link and goes on running frames, so nobody waits. A game that ends the
   loop sooner risks one thing: when the close packet was lost, the other side
   reads TIMEOUT instead of the reason.

All calls are safe when Steam never started, so the game calls them the same
way in every build.

## Verify

```powershell
# without the SDK (the standard features tree)
cmake --build ai_studio/feature_testing/consumer/build/feature-verify --target test_steam_platform test_steam_net_link
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "^test_steam" --output-on-failure
# with the SDK, in a separate tree
cmake -S ai_studio/feature_testing/consumer -B ai_studio/feature_testing/consumer/build/feature-verify-steam -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DSTEAMWORKS_SDK_DIR=<sdk>
cmake --build ai_studio/feature_testing/consumer/build/feature-verify-steam --target test_steam_platform test_steam_platform_dispatch steam_platform_probe test_steam_net_link steam_net_link_contract steam_net_link_probe steam_net_link_closes_live steam_net_link_two_windows
# with a running Steam client: the variable turns a missing client from a skip into a failure
cmake -E env STEAM_PLATFORM_REQUIRE_CLIENT=1 ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify-steam -j4 -R "steam" -L "core|slow" --output-on-failure
# a live run's own log, also the reason of a skip (CTest itself prints only "Skipped" and exits 0)
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify-steam -R steam_net_link_probe -V
# optional window: closes itself; exit 0 only when the overlay showed
cmake --build ai_studio/feature_testing/consumer/build/feature-verify-steam --target steam_platform_overlay_example
node features/validate_contracts.mjs
```

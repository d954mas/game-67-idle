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

The feature builds in any C project. With the SDK it adds one C++ file, and
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
5. Shutdown: destroy anything built on Steam first (a Steam transport), then
   `steam_platform_stop()`, then the window.

All calls are safe when Steam never started, so the game calls them the same
way in every build.

## Verify

```powershell
# without the SDK (the standard features tree)
cmake --build ai_studio/feature_testing/consumer/build/feature-verify --target test_steam_platform
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "^test_steam" --output-on-failure
# with the SDK, in a separate tree
cmake -S ai_studio/feature_testing/consumer -B ai_studio/feature_testing/consumer/build/feature-verify-steam -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DSTEAMWORKS_SDK_DIR=<sdk>
cmake --build ai_studio/feature_testing/consumer/build/feature-verify-steam --target test_steam_platform test_steam_platform_dispatch steam_platform_probe
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify-steam -R "steam" -L "core|slow" --output-on-failure
# the probe's own log, also the reason of a skip (CTest itself prints only "Skipped")
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify-steam -R steam_platform_probe -V
# optional window: closes itself; exit 0 only when the overlay showed
cmake --build ai_studio/feature_testing/consumer/build/feature-verify-steam --target steam_platform_overlay_example
node features/validate_contracts.mjs
```

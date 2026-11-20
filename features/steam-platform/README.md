# Steam platform

## Purpose

The process's connection to Steam behind a pure C header: start, one call a
frame that empties Steam's callback queue, typed events the game pulls, and
stop. The game never includes a Steam header; one C++ file in this feature
does.

It is not a lobby, an invitation flow, achievements or a network transport.
Lobbies and invitations come as later events and calls of this same target.
The transport is a second target of this feature (see Extension points).

### Ways to end up solo

Every one of them is silent: no dialog, no message box, nothing on stdout from
the module. The game gets a status and a short English message for its log,
and the module stays stopped, so every other call is inert.

| Situation | `steam_platform_start` answers |
|---|---|
| Built without the SDK (`STEAMWORKS_SDK_DIR` empty) | `NO_SDK` |
| Built with the SDK, Steam not running or not reachable | `NO_CLIENT` |
| Built with the SDK, Steam's runtime library missing beside the executable (Windows) | `FAILED`, the message names the file |
| Built with the SDK, no Steam installed on the machine | `FAILED` with Steam's text (seen on Linux: Steam cannot load its client module) |

The status follows Steam's own init result, so a game treats every answer but
`READY` and `RESTARTING` as solo and does not tell `NO_CLIENT` from `FAILED`.

Anything else (an outdated client, an account that may not run the app, a
`steam_appid.txt` naming another app than `config.app_id`) is `FAILED` with
Steam's own text. Steam reports ownership only as text, so it cannot be a
status of its own.

`restart_through_steam` is the exception to "solo without Steam": a release
build started outside Steam gets `RESTARTING` and must exit at once. Steam
documents that `SteamAPI_RestartAppIfNecessary` also launches a Steam client
that is not running; a real restart needs the game's own app and was not run
here. Solo without Steam holds for builds that leave the flag off.

### The SDK and its licence

The Steamworks SDK comes from partner.steamgames.com, Downloads, and needs a
Steamworks login. The code needs SDK 1.58 or newer (`SteamAPI_InitEx`); only
1.65 has been built and run.

The SDK never enters git, neither its headers nor its binaries; the Studio
root `.gitignore` refuses stray `steam_appid.txt`, `steam_api*.dll`,
`steam_api*.lib` and `libsteam_api.*`. A developer unpacks the SDK anywhere and
passes its `sdk` folder (the one that holds `public/` and
`redistributable_bin/`) to a separate build tree:

```powershell
cmake -S <game> -B <game>/build/steam ... -DSTEAMWORKS_SDK_DIR=C:/develop/steamworks_sdk_165/sdk
```

Do not set the environment variable `STEAMWORKS_SDK_DIR` globally. The cache
variable defaults from it, so every fresh tree, the standard one too, would
then become the SDK variant, and the stub would no longer be built or tested
anywhere. A set but wrong path is a configure error; only an empty one builds
the stub.

The only SDK files that may leave the developer's machine are the
`redistributable_bin` runtime libraries, copied next to the executable by
`steam_platform_deploy` (`steam_api64.dll` on 64-bit Windows,
`libsteam_api.so`, `libsteam_api.dylib`); see the SDK's `Readme.txt`.

### The runtime library

A build with the SDK needs Steam's runtime library beside the executable. On
Windows it is delay-loaded (`/DELAYLOAD` and `delayimp`, carried to the final
executable by the imported SDK target), and `steam_platform_start` loads it
from the executable's folder before the first Steam call: without it the game
still reaches `main` and `start` answers `FAILED` naming the file, with no
system dialog. On Linux and macOS `steam_platform_deploy` sets an `$ORIGIN`
(`@executable_path`) run path, and the system loader needs the library before
the program runs. 64-bit Windows was built and run against a Steam client.
Linux x86-64 was built with gcc and clang and started on a machine without
Steam, where the library was found beside the executable and `start` answered
`FAILED`. macOS was not built.

### Development on AppID 480

Until the game has its own app, development runs as Spacewar, AppID 480,
through a `steam_appid.txt` beside the executable (`steam_platform_deploy(...
APP_ID 480)`); Steam finds it there. Limits of 480: the signed-in account
appears as playing Spacewar; lobbies on 480 are shared with every developer in
the world, so a lobby search must filter on a key of its own; no achievements,
stats or store data of the game. A release build sets its own AppID, turns
`restart_through_steam` on and ships no `steam_appid.txt`:
`steam_platform_deploy(<target>)` without `APP_ID` removes a stale one after
each build. A development and a release target must not share an output
folder, or each build undoes the other.

The SDK's own `steam_api` library prints two lines on Windows at each start
(`Setting breakpad minidump AppID` and the account's SteamID in full). They
come from Valve's library, not from this module; mind them in shared logs.

## Public surface

`features/steam_platform/steam_platform.h`, C11, no handle (Steam's API is one
per process):

| Call | What it does |
|---|---|
| `steam_platform_start(config, message, size)` | In this order: a NULL config, or `restart_through_steam` with `app_id` 0, is `FAILED` in every build, also while ready (a ready module stays ready); a valid config while ready answers `READY` and changes nothing; otherwise the runtime check, the restart check when asked, init without UI, manual dispatch on. |
| `steam_platform_run_frame()` | Once a frame on the start thread: Steam's periodic work, then every queued callback is read, turned into events or handed to the feature's listeners, and freed; then Steam's per-thread call memory is released. |
| `steam_platform_next_event(&event)` | Pull one event; false when none waits. |
| `steam_platform_user()` | 64-bit SteamID of the signed-in account, zero when not ready. |
| `steam_platform_overlay_ready()` | True once Steam's overlay has hooked the window and is enabled. |
| `steam_platform_open_overlay(dialog)` | `ActivateGameOverlay`; NULL opens `"friends"`, any other name goes to Steam unchanged. |
| `steam_platform_stop()` | `SteamAPI_Shutdown`; drops unpulled events. Safe any time, also twice. |

Events, in a fixed queue of `STEAM_PLATFORM_EVENT_CAPACITY` (16) filled by
`run_frame`; when full the oldest is dropped. No allocation anywhere.

| Event | From | Fields |
|---|---|---|
| `OVERLAY` | `GameOverlayActivated_t` | `active`: shown. Only changes: Steam may repeat a state, the game hears it once. After `stop` the overlay counts as closed. |

Every other callback is freed in the same frame and counted (see
Validation); none is left in Steam's queue.

Programmer errors on the caller's arguments do not crash: a NULL config is
`FAILED`; a NULL or zero-size message buffer is not written; a message is cut
to the buffer and always terminated; a NULL event pointer answers false and
takes nothing; a NULL dialog opens `"friends"`, and what Steam does with an
unknown name is Steam's (not verified).

### The shim

`src/steam_platform.cpp` uses the SDK's C++ interfaces (`SteamUser()`,
`SteamFriends()`, ...) and `SteamAPI_InitEx`, which checks at start that the
running Steam client supports every interface version this build was compiled
against; the flat init (`SteamAPI_InitFlat`) does not. The C++ interfaces tie
the shim to compilers with the MSVC ABI on Windows (MSVC, clang targeting
MSVC); a MinGW build was not tried and is expected to need the flat API
instead. The queue, the config
check and the overlay dialog default live in `src/steam_platform_common.c`,
pure C compiled into both builds.

### Dispatch model

Manual dispatch only: `SteamAPI_ManualDispatch_Init` after init, then each
`run_frame` calls `SteamAPI_ManualDispatch_RunFrame` and loops
`GetNextCallback` / `FreeLastCallback`, and ends with
`SteamAPI_ReleaseCurrentThreadMemory`, which nothing else calls under manual
dispatch. Asynchronous call results arrive as `SteamAPICallCompleted_t` and are
read with `GetAPICallResult` into a fixed 4 KB buffer for whoever awaited that
call. No `STEAM_CALLBACK`, `CCallResult` or `SteamAPI_RunCallbacks`: nothing in
Steam calls the game back, and callback order is the order of `run_frame`'s
loop.

The networking library's callbacks (`SteamNetConnectionStatusChangedCallback_t`,
`SteamRelayNetworkStatus_t`) arrive in the same queue once the networking
interface exists, and are heard by id like any other. The library's
function-pointer callbacks (`k_ESteamNetworkingConfig_Callback_*`, fired by
`ISteamNetworkingSockets::RunCallbacks`) are not used and must not be mixed
in: Steam then reports every change twice, once through each path.

### Threading

Single-threaded by contract: every call from the thread that called `start`.
Steam runs its own threads inside its library; their results reach the game
only through `run_frame` on that thread.

## Validation

- `test_steam_platform` (always built, CTest label `core`): every call inert
  before start and after any failed start; NULL config; `restart_through_steam`
  with app 0 refused in both builds; message cut and termination with guard
  bytes; double stop; the NULL-dialog default; the event queue (changes only,
  oldest dropped when full, NULL `out` takes nothing, reset drops and forgets
  the overlay state). Without the SDK it proves `start` answers `NO_SDK`. With
  the SDK a test hook makes `start` fail without touching Steam, and the test
  proves each init result's status and `RESTARTING`.
- `test_steam_platform_dispatch` (only with the SDK, `core`): real
  `GameOverlayActivated_t` structs fed through the shim's dispatch by a test
  hook: changes only, events kept across `run_frame`, more than the capacity,
  `stop` drops them, short and unknown callbacks counted unhandled.
- `steam_platform_probe` (only with the SDK, label `slow`: it needs a running
  Steam and shows the account as playing Spacewar; exit 77 is a CTest skip):
  a start as app 481 is `FAILED`; then 480, a second start `READY`, a NULL
  config `FAILED` while staying ready; relay status and connection status
  (a local socket pair) by id, a `CheckFileSignature` call result, a second
  await refused, a cancelled call not delivered; stop; a start whose listener
  stops the module from inside `run_frame`; a fresh start where a listener and
  an await deliver again. `--simulate-no-client` takes the skip path on
  purpose. A skip is not proof: CTest prints only `Skipped` and exits 0; the
  reason is in the log, shown by
  `ctest ... -R steam_platform_probe -V` or `--output-junit <file>`.
- `steam_platform_overlay_example` (only with the SDK and the engine, not built
  by default, not in CTest): an engine window that waits for
  `overlay_ready`, opens the friends overlay, waits for the `OVERLAY` event and
  closes itself within 14 s. Exit 0 only when both happened, 2 otherwise, 77
  without Steam.
- `node features/validate_contracts.mjs` checks this contract.

## Compatibility

`feature.json.version` is exact SemVer. A PATCH fixes behaviour to match this
contract without changing it. A MINOR adds calls, event types, event fields at
the end of the struct or config fields with a zero default that keeps today's
behaviour. A MAJOR changes a call, a status or event meaning, the event struct
layout, or what a build without the SDK does. The internal header is not part
of the contract and changes with the feature.

## Extension points

`src/steam_platform_internal.h` is the seam for other translation units of
this feature and its tests; a game never includes it:

- `steam_platform_internal_listen(callback_id, fn, user)`: hear a raw Steam
  callback by its `k_iCallback` during `run_frame`, Steam's struct valid until
  the handler returns. A listener may unlisten or stop from inside; stop takes
  effect after the current callback.
- `steam_platform_internal_await_call(handle, expected_id, fn, user)`: the
  result of one `SteamAPICall_t`, delivered once. Await right after issuing
  the call: a result that completes in a `run_frame` before the await is
  registered is freed as unhandled. A handle already awaited is refused;
  `steam_platform_internal_cancel_call(handle)` forgets one.
- Counters (callbacks seen, unhandled, call results, dropped events) and the
  test hooks `steam_platform_internal_simulate_failure` and
  `steam_platform_internal_feed`.

`steam_platform_stop` forgets every listener and pending call.

The second target, a `net-link` transport on `ISteamNetworkingSockets`, will be
another C++ file here, built only with the SDK, that fills `net_link_ops_t`:
host by `CreateListenSocketP2P`, guest by `ConnectP2P` to a SteamID, and IP
mode on `127.0.0.1` for two windows on one PC. It listens for
`SteamNetConnectionStatusChangedCallback_t` by id to produce net-link's connect
and disconnect events with reasons, maps lanes to `ConfigureConnectionLanes`,
fetches `SteamNetworkingSockets()` again after every start (the interface
pointer changes across a stop and start), and must be destroyed before
`steam_platform_stop`. Lobbies and invitations (`GameLobbyJoinRequested_t`,
`CreateLobby` results) extend this target through new events and the call
results above. Install: [INSTALL.md](INSTALL.md).

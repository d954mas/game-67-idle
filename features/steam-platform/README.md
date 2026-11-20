# Steam platform

## Purpose

The process's connection to Steam behind a pure C header: start, one call a
frame that empties Steam's callback queue, typed events the game pulls, and
stop. The game never includes a Steam header; only this feature's C++ files
do.

Its second target, `steam_net_link`, is a `net-link` transport on Steam's
networking sockets in IP mode: two windows on one PC, or machines on one LAN,
meet over plain UDP with Steam's certificates and its reliability layer, and
the game uses the link through `net_link.h` like any other.

It is not a lobby, an invitation flow or achievements. Lobbies and invitations
come as later events and calls of the first target; relayed P2P as later opens
of the second (see Extension points).

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

### The Steam link

`features/steam_platform/steam_net_link.h`, target `steam_net_link` (exists
where the consumer added `features/net-link` first). It opens links; everything
else is `net_link.h`.

| Call | What it does |
|---|---|
| `steam_net_link_host_ip(config, address, port)` | A host link listening on `port`: on every interface for a NULL `address`, on `"127.0.0.1"` for this machine only (loopback, nothing a firewall filters). NULL for a bad config, port 0, a taken port, a bad address or one that names a port, 8 links open, or a platform not ready. An open right after a host of this process left the port waits for Steam to free it, 250 ms at most. |
| `steam_net_link_join_ip(config, address, port)` | A guest link dialling `address:port`; CONNECTED or DISCONNECTED follows. |
| `steam_net_link_lingering()` | True while a close made through `net_link_close` is in hand (host or guest, link destroyed or not); false by itself within `close_wait_s` plus 3 s (8 s by default), false without the SDK or a ready platform. |
| `steam_net_link_set_fake(send, recv, reorder_ms)` | Steam's process-wide fake loss, lag, reorder and duplicates, clamped. Steam's own shutdown clears them: fake loss set before a stop was gone after the next start, which the probe records. |

Shutdown order: destroy every link, run frames while
`steam_net_link_lingering()`, then `steam_platform_stop()`. A stop cuts every
connection at once: with a guest process that closed and stopped at once, the
host read REMOTE_CLOSE and none of the queued messages, even for a single
16-byte one. With the loop, a 400 KB burst arrived whole and then the close
code, from a guest process and from a host process (`steam_net_link_two_windows`).

How a close is carried out (`src/steam_net_link_closing.cpp`): `net_link_close`
takes the peer away from the link at once (no more events, its place free) and
hands the connection to a module table of closes in hand. Each frame reads
Steam's `GetConnectionRealTimeStatus`; when nothing is pending or unacknowledged
it calls `CloseConnection` with the reason. Steam sends the close once and does
not resend it on a timer: with the close packet dropped, an idle peer never
heard it and timed out after 20 s; when the peer sent something 1.5 s or 3 s
later the closed connection answered with the reason at once, and after 6 s
it no longer did. So the entry stays a short hold after the close: two
keepalives of the link's timeout (an idle peer with the same config sends one
each quarter of it) plus four round trips, at most 3 s. A close not
acknowledged within `close_wait_s` (5 s by default) is closed anyway: its peer
reads the code and loses what was not delivered. The peer leaving or timing
out first frees the entry too.

Steam's own linger (the flag of `CloseConnection`) is never used. A lingering
connection dies without a word when its listen socket closes or the platform
stops. Measured with a host that queued 3.2 MB (12.5 s at Steam's send rate),
closed and was destroyed: left to Steam's linger, its guest read 115 of 200
messages and TIMEOUT after 20 s, and with the port hosted again it stayed
connected and silent for the 45 s it was watched. Closed at the deadline
instead (`close_wait_s` 1 s), the guest read 15 of 200 and the code after
1.0 s. At most 32 closes are in hand; one more closes the one due first at
once. Measured: a small close with a 1 s timeout was done in 0.57 s; 160 KB under 10 % loss and 80 ms
lag in 3.7 s, every message and the code delivered; 400 KB between two
processes with a 2 s timeout in 2.6 s. A host's listen socket stays while a
close on it is in hand, since closing it kills its connections without a word
(proved: 0 of 20 messages and no reason arrived).

The config: lanes and their priorities (Steam sends a lower number first, as
net-link has it), `max_peers` (1..16), `max_reliable_bytes` (1..524288, 16384
by default; Steam's `RecvMaxMessageSize` is set from it, so a peer cannot make
this side buffer a larger message), `send_buffer_bytes` (Steam's
`SendBufferSize`), `timeout_s`, `connect_timeout_s` and `local_without_proof`
(Steam's `IPLocalHost_AllowWithoutAuth`, development only, both ends; with
it on one end only, the connect works or not by whether the certificates are
ready). With it on both ends a fresh child process connected before its
certificate existed and named the host as this account's SteamID.

How it maps, all measured on SDK 1.65 against a running client:

| net-link | Steam |
|---|---|
| peer handle | slot and generation, never reused; Steam's connection user data names the link (registry index and serial) and the peer, set before a connection can deliver anything |
| identity | the SteamID the peer presents, proven by Steam's certificate. Two windows of one account see the same SteamID. Unproven (and so only claimed) under `local_without_proof`; zero for UNREACHABLE and for REFUSED, since a refusing host tells the guest nothing |
| close reason | app codes 1001..1999 unchanged; anything else is sent as 1000 and read as REMOTE_CLOSE, as is Steam's shutdown code 2999; a full host refuses with 2001 (REFUSED); a rule-breaking peer is closed with 2002 (TRANSPORT); Steam's own timeouts read TIMEOUT, or UNREACHABLE before the connect; everything else of Steam's reads TRANSPORT |
| `max_unreliable` | Steam's `MTU_DataSize`, 1200: the largest message that goes in one packet when it is sent alone. Steam's own packet trace showed a message alone in its flush staying in one UDP packet up to 1220 bytes on lane 0 and 1218 on lanes 1..7 (8 lanes), also with acknowledgements flowing back. Steam packs the messages of one flush, so a message behind other data may be split and is then lost when either packet is: under 30 % fake loss a 1200-byte message arrived 0.72 of the time alone and 0.47 behind a 400-byte one (400 tries each; the square of 0.7 is 0.49). A larger unreliable message is refused at send |
| send results | `k_EResultLimitExceeded` is FULL for reliable, DROPPED for unreliable; a dead or unknown peer is NO_PEER |
| timeout | Steam's `TimeoutConnected` counts only while reliable data waits for an acknowledgement; an idle connection was seen to notice silence after Steam's ten-second keepalive plus the timeout (13 s for 3 s). Each end therefore sends an empty reliable message, on its lane that goes first, after a quarter of the timeout without reliable traffic; the other end consumes an empty reliable message on any of net-link's lanes, also one it does not have itself (a host with lanes [1, 1, 0] sends it on lane 2, and a one-lane guest stays connected). A cut is then read within the timeout plus about half a second (2.1 s for 1.5 s); an idle link sent a couple of packets in 2 s at a 10 s timeout |
| close | see above: closed once acknowledged, then a short hold, `close_wait_s` at most for the acknowledgement. A destroyed host keeps its listen socket while a close on it is in hand and turns newcomers away at once (UNREACHABLE, 0.03 s in the probe). Hosting again on the same address and port closes those closes at once (the old guest read its code in 0.01 s, with what it had) and closes the socket. Another address on that port does not end the wait. Steam frees a port a moment after its listen socket closes: an open right after a destroy was refused, one 3 ms later took the port, so an open that follows a close of this module retries for up to 250 ms |
| first connect | a fresh process needs Steam's certificate and keys, one to two seconds after start; a guest waits for them inside `connect_timeout_s` before it dials, as a connect without them fails on the certificate (`Remote_BadCert`). A deadline that passes during that wait reads TRANSPORT (this side could not even try); a certificate that comes inside the deadline is followed by a dial with what is left, which reads UNREACHABLE when nobody answers in time. A fresh process with a 0.25 s deadline whose certificate came later read TRANSPORT after 0.25 s. With `local_without_proof` on both ends a fresh process connected while its certificate was not current |

Received messages are untrusted: one on a lane the link does not have, an
empty one other than the keepalive, an unreliable one over `max_unreliable`
or a reliable one over `max_reliable` never reaches the owner; the peer is
closed and reads DISCONNECTED with TRANSPORT, and the other end reads this
transport's code 2002. A reliable message over `max_reliable` never gets that
far: Steam's `RecvMaxMessageSize` cuts it with Steam's own 5002. Steam itself
also cuts a connection that carries a message on lane 8 or above (5002),
which is why net-link's eight lanes are all Steam has to give and the
keepalive is the empty message.

A keepalive yields no event, so nothing but its own rule bounds it: a peer
may send 16 a second (what the shortest timeout asks for) after a burst of
1024 (what waits in Steam while the owner does not poll). A peer that sends
more is closed with 2002 like any other rule break. A poll that returns zero
has left nothing waiting, as `net_link.h` promises.

Steam's sender prints `steamnetworkingsockets_snp.cpp (2590) :
l.m_cbCurrentSendMessageSent < pNextMsg->m_cbSize` for an empty *unreliable*
message (only the probe's hostile raw peer sends one), once when it is sent
and once when Steam's shutdown frees that connection. Bracketing the probe's
`steam_platform_stop` showed the second print inside it, and it went away
when the raw-peer case was left out; no other case, keepalives included,
prints it. The link's own keepalive is an empty *reliable* message, which
never did in any queue state tried.

Memory: one allocation per open, holding up to 16 peers and 64 of Steam's
messages (those the last poll handed out and those fetched for the next);
nothing of ours per message or event. Steam allocates its own messages.

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
  an await deliver again; hooks (NULL, duplicate and a fifth refused, each
  moment once per hook, `STOPPING` with Steam still callable, a `FRAME` hook
  that stops the module leaves later hooks only `STOPPING`).
  `--simulate-no-client` takes the skip path on purpose.
- `steam_platform_overlay_example` (only with the SDK and the engine, not built
  by default, not in CTest): an engine window that waits for
  `overlay_ready`, opens the friends overlay, waits for the `OVERLAY` event and
  closes itself within 14 s. Exit 0 only when both happened, 2 otherwise, 77
  without Steam.
- `test_steam_net_link` (built wherever `steam_net_link` is, `core`): every
  open answers NULL and `lingering` false without the SDK and, with it, while
  the platform is not started or after a failed start; the config's defaults
  and ranges, the reasons both ways, peer handles and their wrap, connection
  user data, what a received message may be, the keepalive lane, address text
  that names a port, the hold after a close (bounded; extreme inputs that
  would wrap land on the cap), the table of closes in hand (`src/steam_net_link_closes.c`:
  the acknowledgement, the deadline closing with the code, the hold and
  release, the peer leaving first, a full table cutting the close due first,
  an orphaned socket closing with its last close and not before, hosting
  again cutting its closes, forget), the keepalive allowance (the fastest
  honest rate is never refused, a burst is, a long silence earns no more than
  the burst), the certificate
  order (a deadline and a certificate in one frame give up), the event ledger
  (order, CONNECTED before its DISCONNECTED, the first DISCONNECTED kept) and
  the fake values, all without Steam.
- `steam_net_link_contract` (only with the SDK, `slow`, exit 77 skips): the
  16 cases of net-link's contract suite on this transport, host and guests in
  one process over 127.0.0.1, with the cut done by Steam's fake loss.
- `steam_net_link_probe` (only with the SDK, `slow`, 16 cases): an unreliable
  message of `max_unreliable` sent alone leaves in one UDP packet on lane 7 of
  8 by Steam's packet trace and one byte more is TOO_LARGE; a 524288-byte
  reliable message arrives; a full send buffer answers FULL and DROPPED and
  recovers; stats become measured; a raw Steam client that breaks the rules
  seven ways, and a raw host one way, is closed with TRANSPORT, the raw end
  reading 2002 or Steam's own 5002; a guest polls before the frame that would
  bring its "connected" callback and still reads CONNECTED before the host's
  messages; a peer's 50 messages come before its DISCONNECTED when the host
  polls late; a burst of 600 keepalives is consumed by one poll, which
  returns the message behind it, and 3000 more get the raw peer closed with
  2002; the keepalive rides
  the sender's first lane (lane 2 of [1, 1, 0]) and a one-lane guest stays
  connected to it; silence times out within the configured time; address
  text with a port is refused; an idle link sends a handful of packets, not
  one a frame; fake lag shows in the ping and clears; fake loss is gone after
  a stop and a start (Steam's own shutdown clears it; the case records that);
  a link that outlives `steam_platform_stop` reports TRANSPORT once and stays
  inert, and a new start serves new links. `STEAM_NET_LINK_PROBE_ONLY` and
  `_SKIP` take a part of the case names.
- `steam_net_link_closes_live` (only with the SDK, `slow`, 10 cases; filters
  `STEAM_NET_LINK_CLOSES_ONLY` / `_SKIP`): a small close in either role is
  done well under a second; under 10 % loss and 80 ms lag a 160 KB burst,
  close and destroy in either role delivers every message before `lingering`
  turns false, then the code; with everything this process sends dropped,
  `close_wait_s` 1 s ends the wait (still in hand just before it, done within
  it plus the hold), and once the loss is lifted and the guest sends
  something, it reads the code with none of the burst; a peer that
  leaves first ends the close waiting for it in about 0.5 s against a
  `close_wait_s` of 20; two closes in hand on one destroyed host under loss
  both arrive with their own codes; a host close of 480 KB lingers past
  destroy while late guests are turned away within a second; hosting again
  on that port starts at once and the old guest reads its code at once (with
  none of its queue); a destroyed host whose 3.2 MB queue outlasts a 1 s
  `close_wait_s` still delivers its code at the deadline, and the port hosts
  again after it; hosting again right after a destroy gets the port, five
  times in a row; a stop forgets every close in hand.
- `steam_net_link_two_windows` (only with the SDK, `slow`): the test starts
  itself again as guest processes under the same account, in a Windows job
  object that kills them when the test ends on any path: reliable and
  unreliable messages cross on every lane both ways, the guest sees the host
  as the same SteamID, closes with an app code and the host reads it; a guest
  that sends 400 KB, closes, destroys and runs frames while lingering before
  its stop delivers all of it and then its code; a guest that stops Steam with
  its link open reads REMOTE_CLOSE; a guest whose close packet is dropped
  still delivers its code through the hold after the close (with the hold at
  zero the host read TIMEOUT); a killed guest reads TIMEOUT within the
  timeout plus two seconds; a fresh process with a 0.25 s deadline gives up
  within a second, TRANSPORT when its certificate came too late and
  UNREACHABLE when it came in time (watched frame by frame); a fresh process
  with `local_without_proof` on both ends connects before its certificate
  exists; a fresh process whose certificate comes inside a 3 s deadline, with
  nobody hosting, reads UNREACHABLE at the deadline itself (it fails when the
  certificate does not come in time, since the case exists for that branch);
  a host process that sends 400 KB, closes, destroys and lingers before its
  stop delivers all of it and its code to this process.
- `steam_live_no_client_skips` and `steam_live_no_client_fails_when_required`
  (only with the SDK, `slow`: they run the live executables, though they never
  touch Steam): every live run above on its simulated no-client path exits 77,
  and 1 with `STEAM_PLATFORM_REQUIRE_CLIENT=1`.

Each live test prints one line per case with its time. The live tests host
and dial only inside their own block of 200 ports (`STEAM_LIVE_PORT_SHIFT`
moves every block, for two runs on one machine) and fail when they run out of
it, so CTest may run them at once (`-j4`). `STEAM_NET_LINK_CONTRACT_ONLY` runs the contract cases whose
name contains the text.

A skip is not proof: CTest prints only `Skipped` and exits 0, and the reason
is only in the log (`ctest ... -V` or `--output-junit <file>`). A run that
claims Steam was there sets `STEAM_PLATFORM_REQUIRE_CLIENT=1`, and then a
missing client fails every live run:
`cmake -E env STEAM_PLATFORM_REQUIRE_CLIENT=1 ctest --test-dir <tree> -j4 -R steam -L "core|slow"`.
- `node features/validate_contracts.mjs` checks this contract.

## Compatibility

`feature.json.version` is exact SemVer. A PATCH fixes behaviour to match this
contract without changing it. A MINOR adds a target, calls, event types, event
fields at the end of the struct or config fields with a zero default that keeps
today's behaviour. A MAJOR changes a call, a status or event meaning, the event
struct layout, a close code on the wire, or what a build without the SDK does.
The internal headers are not part of the contract and change with the feature.

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
- `steam_platform_internal_hook(fn, user)`: two moments, `FRAME` at the end
  of every `run_frame` (work on time, not on a callback) and `STOPPING` inside
  `steam_platform_stop` while Steam can still be called (release what Steam
  allocated, reset what was set process-wide). A hook lasts one start; there
  is no unhook. A `FRAME` hook may stop the module: the hooks after it then
  hear only `STOPPING`.
- Counters (callbacks seen, unhandled, call results, dropped events) and the
  test hooks `steam_platform_internal_simulate_failure` and
  `steam_platform_internal_feed`.

`steam_platform_stop` calls the `STOPPING` hooks, then forgets every listener,
hook and pending call.

The second target is `steam_net_link`, four C++ files built with the SDK and
a stub without it: `src/steam_net_link.cpp` is one link (open, poll, send,
close; it fills `net_link_ops_t`), `src/steam_net_link_module.cpp` is what the
process has once (the registry that routes
`SteamNetConnectionStatusChangedCallback_t`, heard by id, to a link or to a
close in hand; the `FRAME` hook for the guest's wait on the certificate,
keepalives and closes; the fake network), `src/steam_net_link_closing.cpp`
performs the table of closes in hand on Steam (reads Steam's status, makes
the calls the table asks for), and two pure-C files hold every decision that
needs no Steam, compiled into both builds and tested without a client:
`src/steam_net_link_common.c` (config, reasons, handles, user data, verdicts
on received messages, the keepalive lane, the hold after a close, the ledger
of events poll owes with their order) and `src/steam_net_link_closes.c` (the
table of closes in hand on opaque handles with time passed in, and the order
of the certificate wait). The poll loop itself
stays in the C++ file: it interleaves the ledger with Steam's fetches (what
was fetched goes first, a peer's connection is drained before its
DISCONNECTED, then the poll group). Only the C++ files include Steam's headers. They never cache
`SteamNetworkingSockets()`, whose pointer changes across a stop and start,
never call `ISteamNetworkingSockets::RunCallbacks` and never set the
library's function-pointer callbacks. Relayed P2P (`CreateListenSocketP2P`,
`ConnectP2P` to a SteamID) is the next pair of opens of this target, on the
same link code; lobbies and invitations (`GameLobbyJoinRequested_t`,
`CreateLobby` results) extend the first target through new events and the
call results above. Install: [INSTALL.md](INSTALL.md).

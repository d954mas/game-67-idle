# leaderboard

## Purpose

Capability facade over one leaderboard backend. A game asks what a board can
do, renders what the data carries and hides what the portal cannot serve; it
never branches on a portal name. `SPEC.md` is the contract this pack is
measured against.

## Public surface

`include/features/leaderboard/leaderboard.h` exposes the facade, capabilities,
views, host persistence seam, and backend interface. Games submit scores and
request views without selecting behavior by portal name. See `INSTALL.md`.

## Layer

L1, in-place. Depends on the engine's `nt_base64` and `cJSON`; the portal
backend depends on `features/platform-sdk`.

## Contents

- `include/features/leaderboard/leaderboard.h` — types, constants, the
  game-facing API, `leaderboard_ui_state`, the backend vtable and the
  completion entry points, the `extra` codec.
- `include/features/leaderboard/leaderboard_internal.h` — the pure core
  shared by backends: UTC-day math, payload encoding, response parsing, place
  re-estimation.
- `src/leaderboard.c` — the facade: capability answers, refusal latches,
  per-(board, scope) coalescing, host keys.
- `src/leaderboard_core.c`, `src/leaderboard_extra.c` — the pure core and the
  `k=v;` codec.
- `src/leaderboard_mock.c` — deterministic in-process backend for tests and
  local development.
- `src/leaderboard_http.c`, `src/leaderboard_portal.c` — the two shipped
  backends (see their sections below).
- `tests/` — core tier, pure, no network and no GPU.
- `example/` — a lap-time consumer with an ascending board, generated manifest,
  caller-owned save/backend wiring, explicit screen refresh and a C test.

## What It Owns

- The answer to "what may this screen show": `leaderboard_caps` and
  `leaderboard_ui_state`, per board, answered live by the backend and masked
  by the session's latches.
- Refusal semantics. `UNSUPPORTED` latches for the session and only for the
  capability that refused: a portal refusing writes keeps serving reads.
  `NEEDS_LOGIN` raises the board's login flag and never latches.
  `RATE_LIMITED` retries after backoff. `FAILED` retries within a bounded budget
  and sets `view.error` once it is spent, so the screen can offer a manual retry.
  A login refusal waits for an auth change. HTTP owns its existing polling and
  retry cadence instead of also scheduling facade retries. Read and write
  failures are tracked independently; success on one side cannot clear an
  exhausted error on the other.
- Coalescing per (board, scope): the better value per the board's `sort` is
  kept, a value the portal already accepted is not sent again, and a pending
  value is re-sent at init and on an auth change.
- The anonymous id and the day counters, stored through `leaderboard_host_t`
  under `lb.id`, `lb.day`, `lb.day.<board>` and `lb.sent.<board>.<scope>`.
  Daily requests retain their submission day, so an old-day acknowledgment
  cannot mark a score as sent for the current day.

## What It Does Not Own

- Persistence: the host callbacks are the game's save.
- Any game concept: a skin or a level travels inside the opaque `extra`
  payload, packed and unpacked by the game with the pack's codec.
- Portal JavaScript: that belongs to `features/platform-sdk`.
- The screen: a game draws rows from `leaderboard_view_t` and obeys
  `leaderboard_ui_state`. On opening it, the game calls
  `leaderboard_refresh_now(board)` to request every supported scope. Requests
  made before the SDK is ready remain queued. The game keeps the update pump
  running while the screen is open.

Backends expose startup through the optional `initializing` callback. A
refresh without read capability stays pending only during initialization;
a ready backend that cannot read clears the request.

## Board Manifest

`leaderboards.json` in the game is the single place a board id is written. From
it the build generates the C table handed to `leaderboard_init`, the Playgama
config block, and the checklist of boards a human must create in a portal
console — neither Yandex nor CrazyGames has an API for that.

The manifest names a backend family per publish target (`portal`, `http`,
`none`), not a capability: what a portal can actually do is answered at run
time. Every board on one target uses the same family, including `none`.
Mixed families and duplicate portal IDs are rejected; CrazyGames permits one
effective portal board per game. IDs must be non-empty strings without control
characters, optionally wrapped as `{id, isMain}` with boolean `isMain`.
`INSTALL.md` has the schema, the CMake hook and the commands. A portal that
reports no capability never falls back to HTTP.

## HTTP Backend

`src/leaderboard_http.c`, `include/features/leaderboard/leaderboard_http.h`.
The anonymous self-hosted client for portals without a board of their own:
all-time plus UTC-day from one GET per board, keyed by the facade's player
id, with the place re-estimated locally from the server's top and its bucket
histogram. The endpoint, the xor obfuscation key and the poll cadence arrive
in `leaderboard_http_config_t` from the game; nothing in the pack names a
game, a URL or a key.

- Every request is at once the poll and the submit: the body carries the
  facade's current value for every declared scope, the day it belongs to and
  the last `extra` payload the game submitted. A score submitted while a
  request is in flight goes out the moment that request completes, not on
  the repeat timer.
- Current row metadata remains available even when an acknowledged score is
  deduplicated after relaunch. The service must enforce the board's sort order;
  local place estimation supports both ascending and descending boards.
  Histogram counts represent scores better than the player under that order,
  so ascending boards count smaller scores.
- Cadence: `repeat_delay_s` while healthy, `error_delay_s` for
  `error_fast_tries` failures, then `repeat_delay_s` again. Every failed
  attempt reaches the facade as `FAILED`, so the facade's retry budget decides
  when the screen offers a manual retry.
- A body the parser refuses is a failed request, never an empty board. A
  request that cannot start is `FAILED` too; only a missing endpoint makes
  the board unavailable.
- The server day re-anchors the local clock at day granularity, in both the
  facade (through the page) and the backend's own request day.
- I/O and clocks go through `leaderboard_http_transport_t`; `NULL` selects
  the engine (`nt_http`, `nt_time`, `time`). Tests hand in a canned one.

## Portal Backend

`src/leaderboard_portal.c`, `leaderboard_portal_backend()`. It calls the
platform-sdk leaderboard entry points (`platform_sdk_leaderboard_caps /
submit / fetch / open`, one listener for completions) and contains no
JavaScript: every portal call lives in `features/platform-sdk`.

- A board reaches the portal by `leaderboard_board_def_t.portal_id`; a board
  without one has no capability here. Completions come back keyed by that id
  and are mapped to the board index.
- Capabilities are the portal's, answered live on every call, plus the
  all-time scope only: the portals serve one board per id and never reset it,
  so a declared day scope is dropped, never faked.
- Refusals map one to one onto the platform statuses. Only the platform's own
  `UNSUPPORTED` (no board API on this portal) latches; a request that never
  started because the SDK was not ready stays queued until it can start.
- The backend's `update` pump turns
  two things the facade cannot see — the portal coming up and the player
  logging in — into `leaderboard_backend_auth_changed()`.
- Rows arrive with name, avatar URL and the game's `extra` payload as the
  portal stored it; a payload or URL that does not fit the row is dropped
  whole.

What each portal serves (from `features/platform-sdk/references/contract.md`):
Yandex reads anonymously and writes behind a login, quotas enforced in the
adapter; CrazyGames is write-only with an encrypted score and draws the board
itself; Playgama decides at run time through `bridge.leaderboards.type`;
Poki, itch and local mock have no portal board (the local mock serves canned
rows).

## Avatars

`features/remote-image` supplies the URL-to-texture cache. The game initializes
its capacity and fetch limits, pumps it, and draws either the texture or its
own fallback avatar. This feature does not own avatar art or a renderer.

## Validation

The template links this reusable facade in place. Source tests and the lap-time
consumer exercise local contracts; they do not establish successful calls
against live portal SDKs.
Console setup, vendor authentication, avatar CORS and visual acceptance still
require the portal draft runs described in `SPEC.md`.

## Commands

- `ctest --test-dir templates/template/build/native-debug -R leaderboard --output-on-failure`
- `node --test features/leaderboard/tests/leaderboards.test.mjs` — the manifest validator and generator

## Compatibility

PATCH fixes preserve public signatures and manifest meaning. MINOR additions
preserve existing consumers. MAJOR changes alter public contracts, persisted
host keys, or manifest semantics incompatibly and require consumer migration.

## Extension points

Games supply board manifests, the host persistence callbacks, and presentation.
New services implement the existing backend interface; game-specific ranking
and reward rules remain in the consumer. The lap-time example demonstrates the
integration without a game-specific dependency in the shared module.

## References

- `SPEC.md` — the approved design.
- `INSTALL.md` — wiring a game.

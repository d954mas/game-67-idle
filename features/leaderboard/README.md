# leaderboard

Capability facade over one leaderboard backend. A game asks what a board can
do, renders what the data carries and hides what the portal cannot serve; it
never branches on a portal name. `SPEC.md` is the contract this pack is
measured against.

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

## What It Owns

- The answer to "what may this screen show": `leaderboard_caps` and
  `leaderboard_ui_state`, per board, answered live by the backend and masked
  by the session's latches.
- Refusal semantics. `UNSUPPORTED` latches for the session and only for the
  capability that refused: a portal refusing writes keeps serving reads.
  `NEEDS_LOGIN` raises the board's login flag and never latches.
  `RATE_LIMITED` changes nothing. `FAILED` counts toward a retry budget and
  sets `view.error` once it is spent, so the screen can offer a manual retry.
- Coalescing per (board, scope): the better value per the board's `sort` is
  kept, a value the portal already accepted is not sent again, and a pending
  value is re-sent at init and on an auth change.
- The anonymous id and the day counters, stored through `leaderboard_host_t`
  under `lb.id`, `lb.day`, `lb.day.<board>` and `lb.sent.<board>.<scope>`.

## What It Does Not Own

- Persistence: the host callbacks are the game's save.
- Any game concept: a skin or a level travels inside the opaque `extra`
  payload, packed and unpacked by the game with the pack's codec.
- Portal JavaScript: that belongs to `features/platform-sdk`.
- The screen: a game draws rows from `leaderboard_view_t` and obeys
  `leaderboard_ui_state`.

## Board Manifest

`leaderboards.json` in the game is the single place a board id is written. From
it the build generates the C table handed to `leaderboard_init`, the Playgama
config block, and the checklist of boards a human must create in a portal
console — neither Yandex nor CrazyGames has an API for that.

The manifest names a backend family per publish target (`portal`, `http`,
`none`), not a capability: what a portal can actually do is answered at run
time. `INSTALL.md` has the schema, the CMake hook and the commands.

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
  started because the SDK was not ready counts as `FAILED` and is retried on
  the next trigger.
- The facade re-sends only on a trigger, so the backend's `update` pump turns
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

Not yet written.

## Commands

- `ctest --test-dir templates/template/build/native-debug -R leaderboard --output-on-failure`
- `node --test features/leaderboard/tests/leaderboards.test.mjs` — the manifest validator and generator

## References

- `SPEC.md` — the approved design.
- `INSTALL.md` — wiring a game.

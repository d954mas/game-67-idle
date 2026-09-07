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

Not yet written.

## HTTP Backend

Not yet written.

## Portal Backend

Not yet written.

## Avatars

Not yet written.

## Commands

- `ctest --test-dir templates/template/build/native-debug -R leaderboard --output-on-failure`

## References

- `SPEC.md` — the approved design.
- `INSTALL.md` — wiring a game.

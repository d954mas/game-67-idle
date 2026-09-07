# telemetry

Portal-independent playtest funnel, client side. Only Poki has a game-events
API; Yandex Games and Playgama have none. This module lets a game report the
same funnel on every portal by POSTing small JSON batches to the studio's own
collector (`ai_studio/telemetry`). Without a collector URL it is dormant and
costs nothing.

## Layer

L1 foundation. Depends on the engine's `nt_http` for its default transport
and on nothing above it; the game names the moments, the game persists the
player id.

## Purpose

One funnel for every portal: sessions, playtime, level start/complete/fail
with fight length, merges, purchases, ad offers. Poki's own `measure` stays
in place beside it; this is the copy the studio can read for Yandex and
Playgama too, in the same numbers.

## Public surface

`include/features/telemetry/telemetry.h`:

- `telemetry_config_t` and `telemetry_init(config, transport)`: collector
  URL, key, game, build, platform, player id. Empty URL or NULL transport =
  dormant.
- `telemetry_event_begin(name)`, `telemetry_int/float/str(key, value)`,
  `telemetry_event_end()`: one flat JSON event.
- `telemetry_update(dt, playing)`: heartbeat while playing, interval flush,
  one POST in flight, retry with doubling backoff, give up after three.
- `telemetry_flush()`: send now (page hidden, shutdown).
- `telemetry_make_id(out)`: a 32-hex anonymous id for the game to persist.
- `telemetry_dropped()`, `telemetry_session_id()`, `telemetry_enabled()`.
- `telemetry_http_transport()` (`src/telemetry_http.c`): the `nt_http` POST.

The wire format and the event vocabulary the collector aggregates are in
`ai_studio/telemetry/README.md`.

## Validation

`tests/test_telemetry.c` on an injected transport: wire format, dormancy,
flush and heartbeat cadence, single in-flight rule, retry and give-up,
overflow handling, id shape. Registered by the template and by
example-game as CTest `test_telemetry` (tier core). The collector's own
`node --test ai_studio/telemetry/tests` covers the other end of the wire.

## Compatibility

- PATCH: behaviour fixes that keep the wire format and the header unchanged.
- MINOR: new event fields, new config members with defaults, a new transport.
- MAJOR: a change to the wire format (`v` in the body), to
  `telemetry_config_t` member order, or to what dormant means.

## Extension points

- A transport: implement `telemetry_transport_t` (send/poll/release) for a
  platform without `nt_http`, or for a test.
- A game's funnel module: the moments a game reports are the game's own
  (example-game: `src/systems/sys_portal_metrics.c`); new event names are
  stored raw by the collector and ignored by the report until it learns them.
- Config source: the template reads three CMake cache strings; a game may
  read them from anywhere as long as `telemetry_init` gets the values.

## Behaviour

- One batch open, one POST in flight. A batch holds about a hundred events
  (8 KB); events that do not fit are dropped whole and counted.
- Flush every 15 s when something is buffered, or on `telemetry_flush()`.
- A failed POST is retried with doubling backoff (15, 30, 60 s), three times,
  then dropped. Events buffered meanwhile go out afterwards.
- Native builds without curl get the engine's stub transport: every send
  fails, the module drops and stays quiet.
- No PII. Ids are random hex; the session id is new every launch.

## Install

See `INSTALL.md`.

# telemetry Install

The template links this feature already; a game created from it has the
client compiled in and dormant. These steps are for a game that predates it,
or for turning a dormant client on.

## Install

1. CMake: point at the feature and link its two translation units.

```cmake
set(TELEMETRY_DIR "${GAME_REPO_ROOT}/features/telemetry")
set(TELEMETRY_INC "${TELEMETRY_DIR}/include")
set(TELEMETRY_SRC "${TELEMETRY_DIR}/src")
target_sources(${GAME_TARGET} PRIVATE "${TELEMETRY_SRC}/telemetry.c" "${TELEMETRY_SRC}/telemetry_http.c")
target_include_directories(${GAME_TARGET} PRIVATE "${TELEMETRY_INC}")
target_compile_definitions(${GAME_TARGET} PRIVATE
    GAME_TELEMETRY_URL="${GAME_TELEMETRY_URL}"
    GAME_TELEMETRY_KEY="${GAME_TELEMETRY_KEY}"
    GAME_TELEMETRY_GAME="${GAME_TELEMETRY_GAME}")
```

   `GAME_TELEMETRY_URL`, `GAME_TELEMETRY_KEY` and `GAME_TELEMETRY_GAME` are
   cache strings (`cmake/GameOptions.cmake` in the template). Empty URL keeps
   the client dormant.

2. Save: add the player id to the settings fragment
   (`state/settings.schema.json`), and raise the fragment's `string_max` so a
   32-char id fits:

```json
"string_max": 64,
"telemetry_id": { "type": "string", "default": "new", "max_length": 32 }
```

   The schema forbids an empty string, so `"new"` is the unminted value. The
   settings feature exposes `settings_telemetry_id()`, which mints a real id
   on first use and marks the save dirty.

3. `main.c`: after the save is loaded and the language applied, call
   `telemetry_init` with `telemetry_http_transport()` and send `session_start`
   (the template's `telemetry_start()` is the reference); once a frame call
   `telemetry_update(dt, playing)`; on shutdown `telemetry_flush()` then
   `telemetry_shutdown()`. If the game has a page-visibility hook, call
   `telemetry_flush()` when the page hides.

4. Name the funnel moments in the game's own module (example-game does it
   in `src/systems/sys_portal_metrics.c`, beside the portal's `measure`).

5. Tests: register `features/telemetry/tests/test_telemetry.c` with
   `${TELEMETRY_SRC}/telemetry.c` as a core-tier CTest (see the template's
   `cmake/GameTests.cmake`, target `test_telemetry`), and add
   `${TELEMETRY_SRC}/telemetry.c` plus `${TELEMETRY_INC}` to any test that
   links `settings.c`.

## Turn it on

Deploy the collector once (`ai_studio/telemetry/README.md`), then build the
release lane with the values:

```
cmake ... -DGAME_TELEMETRY_URL=https://europe-west1-<project>.cloudfunctions.net/telemetry -DGAME_TELEMETRY_KEY=<key> -DGAME_TELEMETRY_GAME=<game id>
```

## Verify

- `ctest -R '^test_telemetry$'` green.
- A web build with the URL set: the collector's report shows the session
  within a minute of play (`node ai_studio/telemetry/report.mjs ...`).

## Uninstall

Remove the two sources, the include directory, the three definitions and the
`telemetry_*` calls in `main.c`; drop `telemetry_id` from the schema (the save
tolerates a missing field on load, it does not tolerate an unknown one on a
strict import).

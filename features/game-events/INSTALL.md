# Game Events Install

The default template consumes this feature in-place from `../../features/game-events`.
New games copied from the template inherit that wiring.

## Install

Add module variables near the other in-place feature packs:

```cmake
set(GAME_EVENTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-events")
set(GAME_EVENTS_INC "${GAME_EVENTS_DIR}/include")
set(GAME_EVENTS_SRC "${GAME_EVENTS_DIR}/src")
```

Compile the core event log unconditionally:

```cmake
target_sources(game PRIVATE "${GAME_EVENTS_SRC}/game_events.c")
target_include_directories(game PRIVATE "${GAME_EVENTS_INC}" ...)
target_compile_definitions(game PRIVATE FEATURE_GAME_EVENTS=1)
```

Compile the renderer wherever DevAPI tail or analytics is enabled:

```cmake
target_sources(game PRIVATE "${GAME_EVENTS_SRC}/game_event_render.c")
```

Compile `game_events_devapi.c` only when the engine DevAPI is enabled, and compile
`game_analytics.c` only when local analytics are enabled:

```cmake
if(GAME_DEVAPI_ENABLED)
  target_sources(game PRIVATE "${GAME_EVENTS_SRC}/game_events_devapi.c")
endif()
if(GAME_ANALYTICS_ENABLED)
  target_sources(game PRIVATE "${GAME_EVENTS_SRC}/game_analytics.c")
endif()
```

The template conductor initializes the event spine after `nt_hash_init()` and
before feature initialization:

```c
game_events_init();
/* register typed event labels and descriptor tables */
```

Per frame, the shell owns phase order:

```c
game_features_update(w, dt);
game_events_react_begin();
do { game_features_react(w); } while (game_events_react_progressed());
game_events_set_phase(GAME_EVENT_PHASE_RECORD);
game_features_record(w);
game_event_frame_reset();
```

Shut down analytics first, then the event spine:

```c
game_analytics_shutdown();
game_events_shutdown();
```

## Descriptor Registration

Each producer owns its descriptors and registers them at the conductor site. For
example:

```c
game_ev_register();
game_events_devapi_register_descs(game_ev_descs, game_ev_desc_count);
game_analytics_register_descs(game_ev_descs, game_ev_desc_count);
```

`platform-sdk` is a consumer of this L0 feature. It registers
`platform_sdk_ev_descs` from `features/platform_sdk/platform_sdk_events.h`; the
event spine does not include `platform_sdk.h`.

## Verify

Configure and run the focused native tests:

```powershell
cmake -S templates/template -B templates/template/build/native-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build templates/template/build/native-debug --target test_game_events test_game_events_typed test_game_event_render test_game_analytics test_platform_sdk_events
ctest --test-dir templates/template/build/native-debug -R "test_game_events|test_game_events_typed|test_game_event_render|test_game_analytics|test_platform_sdk_events" --output-on-failure
```

Run full taskboard validation before closing taskboard work:

```powershell
node ai_studio\taskboard\cli.mjs validate --json
```

## Uninstall

Remove the `${GAME_EVENTS_SRC}` target sources, `${GAME_EVENTS_INC}` include
paths, `FEATURE_GAME_EVENTS=1`, conductor lifecycle calls, descriptor
registration calls, and any producers that still emit through `game_event_emit`.

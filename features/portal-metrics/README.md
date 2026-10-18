# Portal Metrics

## Purpose

`portal-metrics` is an opt-in in-place L1 module that drains a game's
per-frame `game_events` log into its own portal funnel mapper. It owns the
cursor that prevents duplicate delivery within a frame and passes direct
measures to `platform-sdk`. Games own event names, dashboard vocabulary,
mapper code, and lifecycle composition.

## Public surface

The public header is `include/systems/sys_portal_metrics.h`, preserving the
consumer include spelling `systems/sys_portal_metrics.h`.

- `sys_portal_metrics_init(map, userdata)` installs one mapper for the process.
- `sys_portal_metrics_record()` drains events added since the previous call in
  the current frame.
- `sys_portal_metrics_measure(category, what, action)` forwards a direct
  measure through `platform-sdk`.

Call `init` before the first event frame and call `record` after every other
reader of the frame log. Passing `NULL` as `map` disables log draining but
leaves direct measures available. The module contains no game event mappings.

## Validation

`tests/test_portal_metrics.c` is a core CTest harness. Its mock SDK proves
once-per-frame mapping, cursor reset after a frame-log reset, direct measure
passthrough, and safe calls before SDK initialization. Consumers include
`cmake/PortalMetricsTests.cmake` and call `portal_metrics_add_core_test()`.

Run the installed consumer's focused test:

```powershell
cmake --build games/<game-id>/build/<preset> --target test_portal_metrics
ctest --test-dir games/<game-id>/build/<preset> -R test_portal_metrics --output-on-failure
```

## Compatibility

PATCH changes preserve exported names, include spelling, singleton mapper
behavior, and delivery semantics. MINOR may add an independent API with a safe
default. MAJOR may remove or rename an exported symbol, change mapper
ownership, or alter event-drain semantics.

## Extension points

Each game supplies its own `portal_metrics_map_fn` and userdata. Mappers may
filter their event types and call `sys_portal_metrics_measure()` for the
dashboard triples they own. A process has one mapper; games that need distinct
funnels compose that routing inside their one mapper.

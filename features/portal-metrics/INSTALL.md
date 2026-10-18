# Portal Metrics Install

## Build

Resolve the in-place module from the Studio root:

```cmake
set(PORTAL_METRICS_DIR "${GAME_REPO_ROOT}/features/portal-metrics")
set(PORTAL_METRICS_INC "${PORTAL_METRICS_DIR}/include")
set(PORTAL_METRICS_SRC "${PORTAL_METRICS_DIR}/src")

target_sources(game PRIVATE "${PORTAL_METRICS_SRC}/sys_portal_metrics.c")
target_include_directories(game PRIVATE "${PORTAL_METRICS_INC}")
target_compile_definitions(game PRIVATE FEATURE_PORTAL_METRICS=1)
```

The consumer must already compile `features/game-events` and
`features/platform-sdk` and expose their include directories.

## Compose

Include `systems/sys_portal_metrics.h`. Initialize once with a game-owned
mapper before event emission. Call `sys_portal_metrics_record()` once at the
end of each frame's event readers. The mapper owns event filtering and portal
vocabulary; it calls `sys_portal_metrics_measure()` for each metric.

## Verify

After defining `PORTAL_METRICS_*`, include the module harness from the
consumer's `GameTests.cmake` and call it after `game_add_c_test` is available:

```cmake
include("${PORTAL_METRICS_DIR}/cmake/PortalMetricsTests.cmake")
portal_metrics_add_core_test()
```

Then build and run `test_portal_metrics` with the commands in `README.md`.

## Uninstall

Remove the lifecycle calls, mapper, module source, include directory, compile
definition, and the focused test-harness include. Keep game-owned analytics
calls only when another mechanism owns their delivery.

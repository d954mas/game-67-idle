---
id: T0022
title: Add clean-seed runtime selection for closed prototypes
status: done
epic: E003
priority: P1
tags: [runtime, cmake, template, cleanup]
created: 2026-06-15
updated: 2026-06-15
---

## What

Restore a clean default game template after closing Splash Rods and Rune
Marches. Closed prototype code and generated assets should remain available for
archive comparison or explicit opt-in builds, but the default native seed should
not load or compile closed game code.

This is not further fishing development. It is template hygiene for the next
prototype.

## Done when

- [x] Default native build uses a clean seed runtime path.
- [x] Closed prototypes can still be built or inspected through an explicit
      opt-in path.
- [x] Default build no longer depends on closed fishing/Rune generated runtime
      assets.
- [x] Validation covers both clean default build and opt-in closed-prototype
      build, or documents why one side is intentionally not built.

## Open questions

- Resolved: use one option, `GAME_CLOSED_PROTOTYPES_ENABLED`, exposed through
  the named `native-debug-closed-prototypes` preset.
- Follow-up: the shared generated state schema still contains fishing/Rune
  fields; split it in `T0024`.

## Log

- 2026-06-15: Created from fishing review finding: `src/main.c` became large
  and closed project runtime/assets remained coupled to the reusable template.
- 2026-06-15: Added default `src/clean_seed_main.c` runtime. It uses the native
  shape renderer, one cycle action, generated state defaults, and minimal
  DevAPI endpoints without including `src/main.c` or `game_state_actions.c`.
- 2026-06-15: Added `GAME_CLOSED_PROTOTYPES_ENABLED` and
  `native-debug-closed-prototypes`. Default `native-debug` now builds
  `src/clean_seed_main.c`; opt-in closed build compiles `src/main.c`,
  `src/game_state_actions.c`, Rune generated assets, Roblox Fishing generated
  UI assets, and the fishing model pack.
- 2026-06-15: Separated output directories:
  `build/game_seed/native-debug/game_seed.exe` for clean seed and
  `build/game_seed/native-debug-closed-prototypes/game_seed.exe` for closed
  prototypes.
- 2026-06-15: Added clean seed output hygiene so stale
  `roblox_fishing_models.ntpack` is removed from the default output. Closed
  opt-in still writes the pack under
  `build/game_seed/native-debug-closed-prototypes/assets/`.
- 2026-06-15: Validation passed:
  `cmake --preset native-debug`;
  `cmake --build --preset native-debug`;
  `cmake --preset native-debug-closed-prototypes`;
  `cmake --build --preset native-debug-closed-prototypes`.
- 2026-06-15: Additional validation passed:
  `node --test tools/game_context/test.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `node tools/pipeline_validate.mjs`.
- 2026-06-15: Verified default compile database had no matches for
  `roblox_fishing`, `rune_marches`, `src/main.c`, or `game_state_actions`.
  The only default build graph match after hygiene was the removal command for
  a stale closed-prototype asset.
- 2026-06-15: Created `T0024` because generated `game_state.c/.h` still
  contains closed prototype fields; this task completed runtime/actions/assets
  split, not the state schema split.
- 2026-06-15: Current-scope profiling guard passed:
  `node tools/ai.mjs status --require-current-scope-usable` reported
  `Current scope review confidence: usable`.

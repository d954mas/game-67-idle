# Graphics Quality Install

## Build

Resolve the in-place module from the Studio root:

```cmake
set(GRAPHICS_QUALITY_DIR "${GAME_REPO_ROOT}/features/graphics-quality")
target_sources(game PRIVATE "${GRAPHICS_QUALITY_DIR}/src/graphics_quality.c")
target_include_directories(game PRIVATE "${GRAPHICS_QUALITY_DIR}/include")
```

## Compose

1. Persist the step and the shadows with the `settings` feature reference
   (`graphics_quality`, `shadows` and their `*_chosen` flags in
   `state/settings.schema.json`). Copy its accessors if the game owns an older
   copy of `settings`. A game whose saves already ship bumps its `settings`
   fragment version with a step that adds nothing: the reader fills the new
   fields' defaults, and the bump stops an older build from rewriting the
   fragment without them.
2. After the graphics context exists and the save has loaded, call
   `settings_adopt_device_graphics()`: it queries the device and fills every
   value the player has not picked.
3. Each frame (or when the setting changes), size the world target with
   `graphics_world_scale(settings_graphics_quality(), device.mobile)` and skip
   the shadow pass and its lookup while `settings_shadows()` is false.

## Verify

```powershell
node ai_studio/feature_testing/consumer/tools/game.mjs test
node features/validate_contracts.mjs
```

## Uninstall

Remove the source and include path, and the calls above. Persisted fields go
through the owning game's normal state-migration policy.

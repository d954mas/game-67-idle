# Scenes Core Install

`scenes-core` is a reusable in-place L1 module.

## In-place wiring

A consumer under `templates/<id>` or `games/<id>` will compile:

```text
../../features/scenes-core/src/scene_manager.c
```

and add this include directory before game-local feature paths:

```text
../../features/scenes-core/include
```

The game owns `game_scenes.c/.h`, its static descriptors/instances, frame
loop calls, scene resources, render membership, and optional DevAPI adapter.
No files are copied from the module into the game.

For DevAPI builds, also compile `src/scene_manager_devapi.c`, include
`features/scenes/scene_manager_devapi.h`, and register the initialized manager.
Treat a `false` registration result as startup failure; it means the command
registry rejected at least one method and may contain methods registered before
the failure. Call `nt_devapi_shutdown()` and do not reuse the partial registry.

## Host calls

The host will:

1. initialize a caller-owned manager and game catalog;
2. call `scene_manager_step` after the engine resource step;
3. call `scene_manager_update` in the game update phase;
4. call `scene_manager_build_ui` inside its existing UI begin/end;
5. guard raw game input and shell UI through manager queries;
6. call manager shutdown before final resource shutdown.

Exact order and UI gating are defined in
[`NEOTOLIS-INTEGRATION.md`](NEOTOLIS-INTEGRATION.md).

## Validation

Validate with:

```powershell
ctest --test-dir templates/template/build/devapi-debug -R "^test_scenes_core_" --output-on-failure
node --test features/scenes-core/tests/test_scaffold_scene.mjs
node --test features/scenes-core/tests/test_scene_devapi_schema.mjs
node --test features/scenes-core/tests/test_consumer_scene_contracts.mjs
node features/validate_contracts.mjs
node --test features/validate_contracts.test.mjs
```

## Uninstall

Remove the module source/include wiring, game-owned catalog/frame calls, scene
DevAPI registration, and dependency record. Game content/state is not owned by
the core and must not be deleted automatically.

# Scenes Core

Status: production-ready for bounded game-shell routing in the default
template; reuse evidence additionally requires the selected private consumer's
own host-lifecycle suite.

## Purpose

`scenes-core` is a reusable in-place L1 route manager for singleton
game scenes. It owns bounded history, lifecycle orchestration, screen/modal
presentation policy, update/input eligibility, and deterministic operation
status. Games own their scene catalog, state, resources, entities, UI, typed
route endpoints, and debug/capture behavior.

The normative design is
[`SCENE-MANAGER-SPEC.md`](SCENE-MANAGER-SPEC.md); the gated delivery sequence is
[`IMPLEMENTATION-PLAN.md`](IMPLEMENTATION-PLAN.md).

## Public surface

The public header is
`include/features/scenes/scene_manager.h`. Its surface includes catalog
lookup, preload residency, deferred `start/show/replace/reload/back/back_to/
close_modals`, update/UI dispatch, input/visibility queries, operation status,
and shutdown.

The optional `scene_manager_devapi.c` adapter registers strict `game.scene.*`
automation. Parameterized scenes keep a game-owned typed endpoint.

## Validation

Core and scaffold validation:

```powershell
cmake --build templates/template/build/devapi-debug --target test_scenes_core_catalog test_scenes_core_lifecycle test_scenes_core_navigation test_scenes_core_ordering test_scenes_core_presentation test_scenes_core_deadlines test_scenes_core_reentrancy test_scenes_core_devapi
ctest --test-dir templates/template/build/devapi-debug -R "^test_scenes_core_" --output-on-failure
cmake --build templates/template/build/wasm-release-itch --target test_scenes_core_web_smoke
ctest --test-dir templates/template/build/wasm-release-itch -R "^(test_scenes_core_web_smoke|scenes_core_)" --output-on-failure
node --test features/scenes-core/tests/test_scaffold_scene.mjs
node --test features/scenes-core/tests/test_scene_devapi_schema.mjs
node --test features/scenes-core/tests/test_consumer_scene_contracts.mjs
node features/validate_contracts.mjs
node --test features/validate_contracts.test.mjs
```

## Compatibility

Before `1.0.0`, the reviewed specification is the compatibility boundary.

The fixed caller-owned storage overlay supports Clang/GCC through `may_alias`
and MSVC's C aliasing model. Unknown compiler families fail at compile time
rather than silently relying on unsupported effective-type behavior.

- PATCH clarifies docs/tests or fixes behavior without changing the public
  contract.
- MINOR adds backward-compatible capability after a reviewed real use case.
- MAJOR may change signatures, state-machine behavior, storage limits, or
  lifecycle semantics and requires renewed consumer migration.

Version `0.3.0` is the reviewed runtime line and remains pre-1.0. It closes
the lifecycle-dispatch reentrancy hole, adds an explicit adapter-level
`NOT_TOP` result, and promotes callback-order and consumer wiring checks into
the required suite.

## Extension points

Games extend the module through static scene descriptors and callbacks,
game-owned catalogs, typed parameterized DevAPI endpoints, render membership,
and optional transitions. Dedicated packs are a manual scene/host optimization.

Preload is also a residency commitment: a scene that reaches READY before its
first presentation remains READY until it is shown and later hidden or the
manager shuts down. There is no background eviction sweep, so consumers must
budget heavy preloads explicitly.

Multiple runtime instances, navigation queues, resource-pack policy, raw-input
bubbling, state serialization, and capture protocols remain outside V1.

## Agent scaffold

Preview or add a scene without generating a resource pack:

```powershell
node features/scenes-core/scripts/scaffold_scene.mjs --project templates/template --id debug.recording --debug
node features/scenes-core/scripts/scaffold_scene.mjs --project templates/template --id debug.recording --debug --apply
```

Debug scene catalog entries and sources compile only when the consumer is
configured with `-DGAME_DEBUG_SCENES_ENABLED=ON`. This flag is independent of
`GAME_DEVAPI_ENABLED`.

The game catalog and CMake file contain explicit scaffold anchors. The command
preflights every anchor and existing destination before writing, rolls back a
handled partial commit, and returns one JSON result. It deliberately generates
a parameterless scene; typed route endpoints and scene-specific tests remain
game-owned follow-up work.

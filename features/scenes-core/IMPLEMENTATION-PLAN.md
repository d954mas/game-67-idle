# Universal Scene Manager Implementation Plan

Status: FINAL 0.3.0 IMPLEMENTATION AND MIGRATION VERIFIED 2026-07-25

Implementation note: seven focused native core targets plus DevAPI, WebAssembly,
tooling, and consumer-contract suites are the permanent verification surface.
The trolley game additionally owns a real host-lifecycle executable.

Contract:
[`SCENE-MANAGER-SPEC.md`](SCENE-MANAGER-SPEC.md)

The plan delivers a reusable in-place L1 module, then proves it in the template
and one additional real consumer. Each phase has a mechanical exit gate.

## Global constraints

- Core lives and compiles in place from `features/scenes-core/`.
- Template/game owns catalog, scenes, render membership, typed route endpoints,
  and composition.
- Use only public Neotolis APIs; engine worktree remains read-only.
- No heap, multiple scene instances, pack abstraction, state serialization,
  navigation queue, or input-event framework.
- Contract violations abort in every build profile; there is no production
  assertion recovery mechanism.
- Any core contract change after Phase 0 returns to independent review.

## Planned file/target map

Core:

```text
features/scenes-core/
  README.md
  INSTALL.md
  feature.json
  include/features/scenes/scene_manager.h
  include/features/scenes/scene_manager_devapi.h
  src/scene_manager.c
  src/scene_manager_devapi.c
  schemas/scene-devapi.v1.schema.json
  scripts/scaffold_scene.mjs
  tests/test_scene_manager_catalog.c
  tests/test_scene_manager_lifecycle.c
  tests/test_scene_manager_navigation.c
  tests/test_scene_manager_ordering.c
  tests/test_scene_manager_presentation.c
  tests/test_scene_manager_deadlines.c
  tests/test_scene_manager_reentrancy.c
  tests/test_scene_manager_devapi.c
  tests/test_scene_manager_web_smoke.c
  tests/test_consumer_scene_contracts.mjs
  tests/test_scene_devapi_schema.mjs
  tests/test_scaffold_scene.mjs
```

Template-owned integration:

```text
templates/template/src/game_scenes.c
templates/template/src/game_scenes.h
templates/template/src/scenes/<scene-id>.c/.h
templates/template/CMakeLists.txt
templates/template/cmake/GameTests.cmake
templates/template/game-dependencies.json
features/README.md
features/validate_contracts.mjs
features/validate_contracts.test.mjs
```

Core CTest targets:

```text
test_scenes_core_catalog
test_scenes_core_lifecycle
test_scenes_core_navigation
test_scenes_core_ordering
test_scenes_core_presentation
test_scenes_core_deadlines
test_scenes_core_reentrancy
test_scenes_core_devapi
test_scenes_core_web_smoke
```

## Phase 0 - Contract and feature-packaging gate

Deliver:

1. Final spec, integration profile, comparison, implementation plan, and
   `PLAN-REVIEW.md`.
2. `README.md` router sections, `INSTALL.md`, and a pre-1.0 `feature.json`.
3. `scenes-core` entry in `features/README.md` and the template dependency seed.
4. Feature-contract validation wiring.
5. DevAPI schema aligned to the actual Neotolis transport: strict handler
   `params`, optional transport envelope fields, actual error codes.
6. Remove capture-specific schema/types from the core feature.

Mechanical gate:

```powershell
node features/validate_contracts.mjs
node --test features/validate_contracts.test.mjs
```

Exit:

- independent final verdict is GO;
- no contract/schema contradiction or missing feature package file;
- user authorization is the only trigger to begin Phase 1.

## Phase 1 - Storage, catalog, history, and assert test harness

Deliver:

- fixed caller-owned `scene_manager_t` storage;
- 64-scene/128-history/64-byte-args limits;
- catalog initialization and descriptor validation;
- singleton records, history entries, safe lookup, deterministic snapshots;
- memcpy-only route argument helpers and lifetime documentation;
- focused assert-trap fixtures.

Tests:

- duplicate/invalid/capacity assertions;
- unknown safe lookup;
- 0-byte/64-byte/NULL/size rules;
- identical parameterized history entries remain distinct;
- repeated ids share one singleton;
- no heap allocation and deterministic catalog ordering.

Exit:

```powershell
cmake --build <build-dir> --target test_scenes_core_catalog
ctest --test-dir <build-dir> -R "^test_scenes_core_catalog$" --output-on-failure
```

No Neotolis resource, renderer, UI, input, or game dependency exists in core.

## Phase 2 - Residency, loading scheduler, preload, and shutdown

Deliver:

- UNLOADED/LOADING/READY state machine;
- deterministic one-step-per-LOADING-scene scheduler;
- per-scene load generation/start/deadline;
- per-scene preload request bit and no-operation-id result;
- no background eviction sweep;
- synchronous unload for READY and partially LOADING scenes;
- resident-state shutdown unload-once behavior;
- `keep_loaded` residency primitive.

Tests:

- multiple preloads progress in catalog order;
- repeated preload scheduled/loading/ready results;
- READY preload remains until shutdown when never presented;
- scene-owned known failure asserts in its fake; manager deadline/illegal state
  asserts in core;
- retained, absent-preloaded, and partially-loading shutdown each unload
  exactly once.

Exit:

```powershell
cmake --build <build-dir> --target test_scenes_core_lifecycle
ctest --test-dir <build-dir> -R "^test_scenes_core_lifecycle$" --output-on-failure
```

## Phase 3 - Navigation, presentation diff, and operation diagnostics

Deliver:

- all navigation calls copy one pending intent for next step;
- pending/active/last-completed operation status;
- `start/show/replace/reload/back(count)/back_to/can_back_to/close_modals`;
- candidate history plus old/new visible diff;
- entering/staying/leaving/reactivated classifications;
- explicit recreated-focus path for same-id Replace/Reload;
- atomic callback/commit order;
- transition states and no-op transition path from the beginning;
- same-id replace/reload yield one manager step after unload.

Tests:

- every screen/modal legality rule and root protection;
- modal-root assertion occurs when Start activates;
- Show attaches to a target already LOADING from preload;
- background preload never occupies or blocks the pending/active navigation
  slot;
- after a preloaded scene is shown, its first ordinary hide applies the normal
  `keep_loaded` policy;
- screen→modal, modal→screen, stacked modal, close-all, and large atomic Back;
- parameterless same-top and empty close-all ALREADY_TOP;
- adjacent and non-adjacent repeated singleton args, including
  `[A(args1), B, A(args2)] -> back(2)`;
- nearest previous Back-to and missing-target assertion;
- exact callback/query history observations around commit;
- only focused leaving/reactivated exit and entering/reactivated enter;
- screen/modal Reload force the recreated lifecycle trace;
- second pending/active command returns BUSY with blocker id;
- lifecycle callback reentrant navigation asserts;
- shutdown in every pending/active non-transition navigation phase abandons the
  operation and unloads each singleton once;
- older-than-last-completed operation id is not found.

Exit:

```powershell
cmake --build <build-dir> --target test_scenes_core_navigation
ctest --test-dir <build-dir> -R "^test_scenes_core_navigation$" --output-on-failure
```

Every operation has an exhaustive trace test before host integration.

## Phase 4 - Update, modal policy, UI modes, input queries, and transitions

Deliver:

- bottom-to-top visible update and UI order;
- modal-suffix PAUSE/CONTINUE aggregation;
- exact staying-scene pause/resume edges;
- immediate pending/active input gate;
- `can_process_input` and host `input_gated` query;
- INTERACTIVE/PASSIVE UI modes;
- enter/exit `step(dt)` and deterministic deadline;
- render-visibility query needed by host systems.

Tests:

- hidden scenes never update/build UI;
- any PAUSE modal stops ordinary update; all CONTINUE permits it;
- leaving scene gets hide without artificial resume;
- accepted command immediately blocks raw input and makes UI passive;
- visible source keeps updating while target loads;
- transition time uses supplied manual `dt`;
- transition callback reentrant navigation asserts;
- shutdown during enter/exit transition does not finish the transition and
  unloads each singleton once;
- retained hidden scene is excluded from presentation iteration.
- screen/modal Reload dispatch no update/UI/render callback during their loading
  gap;

Exit:

```powershell
cmake --build <build-dir> --target test_scenes_core_presentation
ctest --test-dir <build-dir> -R "^test_scenes_core_presentation$" --output-on-failure
```

No `on_draw`, raw-input payload, consumed flag, or UI framework dependency has
entered core.

## Phase 5 - Neotolis template integration

Follow
[`NEOTOLIS-INTEGRATION.md`](NEOTOLIS-INTEGRATION.md).

Deliver:

- explicit manager step/update/UI placement in the current frame loop;
- game-owned root/settings catalog;
- render membership activation/deactivation;
- shared-pack lifecycle and optional dedicated-pack proof fixture only;
- disabled PASSIVE widgets, focus reconciliation, and a UI gate declared after
  scene/global UI;
- audit and allowlist of every direct `nt_input`/`g_nt_input` reader;
- guarded `sys_move` and manager-aware shell Escape;
- host loading surface for same-id reload/replace;
- shutdown resource step after scene unload.

Tests:

- source/global UI cannot click through during target load/enter/exit;
- opening pointer sequence, disabled focused text input, and modal
  Escape/focus cases;
- hidden retained entities do not render;
- shared resource handles remain resident while scene runtime unloads;
- optional pack unmount is followed by resource step before remount;
- native/web template navigation and shutdown smoke;
- existing targeted engine tests stay green without engine edits.

Exit:

```powershell
cmake --build <build-dir> --target test_game_scenes_integration
ctest --test-dir <build-dir> -R "^test_game_scenes_integration$" --output-on-failure
```

The template still owns its root, frame loop, game model, and renderer.

## Phase 6 - Strict game-owned automation adapter

Deliver:

- strict `game.scene.*` params/result schema matching actual transport;
- list/status/operation status;
- unknown-id preflight;
- parameterless generic Show/Replace;
- typed game-owned endpoint convention for parameterized scenes;
- stack status containing id/argsSize only;
- preload scheduled/no-op response with residency polling;
- pending/active/last-completed operation polling and BUSY blocker id;
- independent `GAME_DEBUG_SCENES_ENABLED` and `GAME_DEVAPI_ENABLED` flags.

Tests:

- unknown/duplicate/missing/wrong-type/range fields reject before mutation;
- generic route rejects parameterized scene with `typed_endpoint_required`;
- typed endpoint constructs a local C value and binary API copies it;
- no JSON-to-memory casts, temporary pointers, or raw history bytes;
- manual time drives navigation terminal status;
- real Neotolis envelope behavior and error codes;
- release excludes debug endpoints independently of DevAPI choice.

Exit:

```powershell
cmake --build <build-dir> --target test_game_scenes_devapi
ctest --test-dir <build-dir> -R "^test_game_scenes_devapi$" --output-on-failure
```

Capture actions/readiness/hash protocols remain separate game/runtime-automation
extensions.

## Phase 7 - Agent scaffold and debug reference scene

Deliver:

- generator dry-run/check/apply workflow;
- preflighted transactional edits for scene files, game catalog, and CMake
  source composition;
- parameterless generated scene contract;
- optional structurally excluded debug source/catalog;
- machine-readable JSON result for agent use;
- one debug/recording reference scene with ordinary scene lifecycle;
- `README/INSTALL` add-scene, remove-scene, and manual dedicated-pack recipe.

Tests:

- generate → check → compile;
- repeated apply is idempotent or fails before mutation;
- missing/changed catalog or CMake anchors cause zero writes;
- release catalog/source composition contains no debug scene.

Exit:

- `node --test features/scenes-core/tests/test_scaffold_scene.mjs` passes;
- generator fixture native compile and release-inspection smoke pass;
- a generic agent can add a parameterless test scene without editing core;
- parameterized scene types/endpoints/tests remain explicit game-owned work;
- no pack is generated.

## Phase 8 - Second-consumer proof

Before this phase starts, the user selects one available real consumer. The
tracked plan does not hard-code a private game id.

Deliver:

- independent game-owned catalog with screen, stacked modal, repeated
  parameterized route, and debug/recording scene;
- migration without a consumer-specific core branch;
- measured decision whether any scene merits a dedicated pack.

Tests:

- navigation/reload/back/back_to/modal traces;
- current game-model data after history return;
- deterministic automation entry to a recording scene;
- native/web debug/release smoke.

Exit:

- no core API change was required;
- if one is required, return to Phase 0 and repeat architecture review.

## Phase 9 - Final verification and version

Run:

- all four core test targets plus the normal template and consumer suites;
- `node --test features/scenes-core/tests/test_scaffold_scene.mjs`;
- feature-contract tests;
- template and selected-consumer native/web debug/release builds;
- release source/catalog/symbol/endpoint/asset inspection;
- documentation link/diff checks.

Record evidence for:

- lifecycle and presentation-diff ordering;
- preload concurrency and reload resource boundary;
- modal raw/UI input isolation;
- repeated route arguments;
- debug release exclusion;
- automation determinism;
- unload-once shutdown.

Assign a stable feature version only after all evidence passes.

## Separate optional engine work

Selective activation/unload of resources inside one `.ntpack` stays an
independent engine issue and PR. It neither blocks nor expands this plan.

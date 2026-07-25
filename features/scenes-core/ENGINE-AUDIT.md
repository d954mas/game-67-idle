# Neotolis Scene-System Capability Audit

Status: REVIEWED ENGINE EVIDENCE; POLICY CONCLUSIONS SUPERSEDED BY CURRENT SPEC
Audited engine: Neotolis `0.1.0`, gitlink
`ffab834c72d1c50b5a5a2ea628521bddab252ccc`
Audit date: 2026-07-23

This audit answers one question: can the current public Neotolis API support the
proposed universal scene manager without modifying the engine?

## Verdict

Yes, with constraints.

The engine already supplies pack loading/unmounting, resource publication,
global input snapshots, one application frame callback, and modal UI
primitives. It does not supply a scene lifecycle, navigation stack, input
consumption between scenes, resource ownership/refcounts, or automatic
dependency closure. Those responsibilities must remain in `scenes-core` and
the consuming game.

No engine change is required for v1. The current specification uses the
existing shared game pack by default. Pack-granular residency is available as
an optional scene/host optimization when independent unload has measured value.
Selective load/unload of assets inside one pack remains an independent engine
capability request.

Constraints when a consumer optionally adds dedicated packs:

- disjoint resource ids across concurrently mounted ordinary packs;
- assertion on a deterministic deadline when generic activation failure cannot
  be observed directly;
- a synchronous native file-read portion during load;
- one shared UI context with passive scene builders plus a host-owned input
  gate;
- game-owned scene pause and input scheduling.

If any of those constraints is rejected, the required engine enhancement must
be proposed through an engine issue and PR. The engine subtree stays read-only.

## Resource findings

### Multiple packs and global publication

Multiple `.ntpack` files can be mounted. Every mounted asset participates in a
single global winner calculation. Priority wins first and later mount wins an
equal-priority tie:

- `external/neotolis-engine/engine/resource/nt_resource.c:228`
- `external/neotolis-engine/engine/resource/nt_resource.c:249`

Loading a pack activates every registered asset as budget permits, not merely
the ids explicitly requested by a scene:

- `external/neotolis-engine/engine/resource/nt_resource.c:578`
- `external/neotolis-engine/engine/resource/nt_resource.c:650`

Consequences:

- scene packs mounted at the same time must have disjoint `resource_id` sets;
- shared ids must exist only in `shared.ntpack`;
- undeclared cross-pack collisions must fail the build;
- overlay/hot-swap packs are a future centrally owned feature, not v1 behavior.

### Requests, readiness, and dependency closure

`nt_resource_request()` creates a persistent publication slot. There is no
public release/unrequest/refcount API, and slots live until resource shutdown.

Pack `READY` means parsing/registration has completed. It does not prove that
every resource required by a scene's first frame is published and usable.
Readiness is type-specific:

- texture, mesh, shader, font: `nt_resource_is_ready(handle)`;
- atlas: wait for atlas readiness, then query/cache page handles, then wait for
  every mandatory page;
- blob-backed manifest: readiness plus a non-null blob, followed by copying any
  data that must outlive blob eviction;
- game-owned runtime object: a scene-specific validator.

The atlas page count accessor may assert if used before atlas resolution, so
page enumeration is explicitly a second stage:

- `external/neotolis-engine/engine/atlas/nt_atlas.c:568`
- `external/neotolis-engine/engine/atlas/nt_atlas.c:645`

Neotolis has no public model/material/atlas dependency graph that can answer
"everything this scene needs." The scene must know and validate its mandatory
closure.

### Failure observability

Immediate pack failure is public through `NT_PACK_STATE_FAILED`. A scene can
also report an immediate mount/request error or a game-specific validation
error.

Generic asset activation failure is not safely distinguishable from "still
pending" through the stable public API: the symbolic asset failure state is
internal, while public `nt_resource_get_state()` returns a raw byte. Therefore
v1 uses a manager-owned deterministic deadline as the final assertion guard.
There is no scene-manager recovery or retry path after a mandatory load failure.

If immediate generic asset failure is required, the engine needs a public
failure predicate or public asset-state enum.

### Unmount barrier

Unmount cancels pack I/O and deactivates file-pack runtime objects immediately,
but requested slots reconcile their published winner only in the next
`nt_resource_step()`:

- `external/neotolis-engine/engine/resource/nt_resource.c:803`
- `external/neotolis-engine/engine/resource/nt_resource.c:860`
- `external/neotolis-engine/engine/resource/nt_resource.c:700`
- `external/neotolis-engine/engine/resource/nt_resource.c:716`

Consumers stop before unmount begins. The revised core has no serial API:
`unload()` is synchronous, the operation yields, and the Neotolis host always
executes `nt_resource_step()` before the next `scene_manager_step()`. Therefore
a same-scene reload/remount cannot run before the required publication step.

### Capacity and I/O

Default hard limits:

- mounted packs: 16;
- registered asset entries: 2,048;
- persistent requested slots: 2,048;
- file-system requests: 8;
- HTTP requests: 8.

Several resource capacity paths assert rather than return a recoverable error.
The public API also does not expose the persistent slot count. Therefore
runtime checks alone are insufficient. The build must prove:

- maximum simultaneous pack residency is at most 16;
- simultaneous mounted asset entries are at most 2,048;
- the union of every id ever requested, including atlas pages and placeholders,
  is at most 2,048.

Background scene loading may overlap. A scene that mounts an optional pack must
handle immediate capacity/start failure and assert with context. Consumers with
several dedicated packs also need generated build-time capacity proofs.

### Native versus web loading

Web pack transfer is asynchronous and exposes byte progress. Native
`nt_resource_load_auto()` performs `fopen`/`fread` synchronously before later
activation steps:

- `external/neotolis-engine/engine/resource/nt_resource.c:1367`
- `external/neotolis-engine/engine/fs/native/nt_fs_native.c:13`
- `external/neotolis-engine/engine/fs/native/nt_fs_native.c:58`

Thus native preload is pollable after the initial read, but it is not guaranteed
to be nonblocking. Also, the current template and trolley hosts configure an
activation budget of zero, which means unlimited activation. Frame-progressive
activation requires an explicitly approved finite budget.

## UI, input, and application findings

### Application ownership

Neotolis exposes one application frame callback. The game/host owns ordering
between resource stepping, scene transactions, updates, UI building, and draw.
The scene manager must not assume separate engine update/render callbacks.

### Input

Input is a global snapshot. There is no public consume/route API between scenes.
UI capture and focus are per `nt_ui_context_t`; two contexts can both inspect
the same global snapshot.

Consequences:

- the manager exposes one game-input owner, but the host dispatches its own
  input type;
- covered scenes must not poll global input directly;
- skipping a lower scene's game-input callback does not disable stock
  immediate-mode widgets that are still declared;
- global `nt_app_pause()` is not a scene-pause mechanism because it also freezes
  the modal's time.

### `nt_ui_modal`

`nt_ui_modal` supplies panel/backdrop layout, same-context pointer arbitration,
and tween state. It does not own:

- scene stack/history;
- scene resource lifecycle;
- scene update pause;
- keyboard/game-hotkey routing;
- cross-context input exclusion.

Its arbitration uses previous-frame geometry/top-modal state. A new modal has
an establishment frame during which helper close/hit-testing is not yet fully
authoritative. Manager input ownership must switch as soon as a modal request
is accepted.

V1 therefore selects one shared UI context. Covered/gated scenes receive a
`PASSIVE` UI build mode and do not act on widget results. The focused stable
scene receives `INTERACTIVE`. During navigation the host declares a final
full-screen modal/pointer catcher after all scene and global UI.

If a product requires arbitrary unchanged interactive lower-scene UI to remain
declared under a modal, the engine needs per-context input enable/focus control.

## Pack-pipeline reality

The current template build assumes exactly one `game.ntpack` across:

- builder contexts and generated headers;
- CMake outputs/dependencies;
- native/web runtime paths;
- web copying/serving;
- release required-file and ZIP checks.

Scene-exclusive unload is impossible until this whole pipeline supports
multiple packs. A runtime-only change is insufficient.

## Verification performed

A separate engine test build was configured at
`build/neotolis-scene-audit`. The following targeted tests passed:

```text
test_resource
test_nt_ui_modal
test_app
test_input
```

Command:

```powershell
ctest --test-dir build/neotolis-scene-audit `
  -R "^(test_resource|test_nt_ui_modal|test_app|test_input)$" `
  --output-on-failure
```

Result: 4/4 passed. This verifies current engine behavior, not the future scene
manager.

## Shutdown order

The host shutdown contract is:

1. stop and destroy scene consumers;
2. run scene unload callbacks;
3. when an optional dedicated pack was unmounted, execute its required later
   resource step;
4. shut down the scene manager;
5. call `nt_resource_shutdown()`.

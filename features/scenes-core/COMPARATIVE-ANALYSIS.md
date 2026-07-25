# Scene Manager Comparative Analysis

Status: FINAL DECISION RECORD FOR VERSION 0.3.0

Compared contract:
[`SCENE-MANAGER-SPEC.md`](SCENE-MANAGER-SPEC.md)

Research snapshot: 2026-07-24

## 1. Conclusion

The proposed feature is best described as an application route manager with
scene lifecycle hooks. Mature engines normally split this responsibility:

- world/scene composition;
- resource loading and residency;
- UI/navigation history;
- input focus;
- game-specific state.

Therefore `scenes-core` belongs in the reusable game-feature layer, not inside
Neotolis. The game should continue to own its catalog, scene objects, resources,
and runtime content.

The current contract combines what the two accepted consumers need:

- stable scene ids and navigation history;
- one singleton scene object per id;
- repeatable history occurrences with immutable small route arguments;
- scene-owned cooperative staged readiness and synchronous unload;
- modal presentation/update/input policy;
- deterministic commands and status for agents.

## 2. Comparison

| System | Runtime model | Navigation history | Loading/resources | Relevant lesson |
|---|---|---|---|---|
| Local Defold manager | Singleton wrapper around collection proxy | Yes; screens and modals | Proxy owns collection dependency lifecycle | Proven behavioral ancestor, but lower-scene target-id policy and proxy workarounds should not become universal API |
| Defold collection proxy | Dynamically loaded collection world | No product back stack | `load/init/enable`, then `disable/final/unload` | Supports scene-owned lifecycle; navigation policy remains application-owned |
| Unity SceneManager | Loaded scene instances, additive or replacing | No product back stack | Async load/unload; Addressables adds handle/ref-count ownership | Separates world instances from navigation and resources; do not make the manager a pack wrapper |
| Godot SceneTree | Instanced node trees | No built-in product history | Cached/ref-counted resources and background loading | Full scene replacement is ordinary teardown/recreate; state restoration is game responsibility |
| Unreal | Levels/worlds plus CommonUI widget stacks | UI stacks exist separately from levels | Asset Manager/streaming are separate systems | Navigation, world streaming, resource ownership, and input focus deserve separate boundaries |
| Phaser | Registered singleton-like scene systems with lifecycle | Manager operations, no automatic product back history | Each scene can load assets; cache lifetime is separate | Closest lifecycle shape, including deferred operations, but app history still belongs above it |
| Bevy | App states and ECS schedules | No scene history | Asset handles/caches are separate | Update eligibility can be state-driven without inventing a scene renderer |

### 2.1 Local alternatives and costs

| Dimension | `scenes-core` | Enum/switch | Classic scene stack | ECS state/tags | Engine-owned router |
| --- | --- | --- | --- | --- | --- |
| Ownership | Singleton catalog/history/lifecycle policy; content remains game-owned | Entirely game-owned | Stack usually owns per-entry instances | World owns persistent entities and schedules | Engine owns graph/router |
| History | Bounded occurrences with copied args; atomic back/back-to/close | Manual | Natural per-instance push/pop | Separate router required | Potentially built in |
| Overlay model | One screen plus top modal visible | Manual | Arbitrary stack layers | Separate UI layer | Usually arbitrary graph/layers |
| Loading | Cooperative `load_step`, sync unload, deadline assert; no failure result, cancellation, or retry | Bespoke | Commonly coupled to each instance | Usually global asset state | Richest when engine supports it |
| Input/UI | Focused top, PASSIVE builders, host gates; no bubbling | Manual | Top-first/bubbling is natural | System-order policy | Often integrated focus routing |
| Memory | Fixed 32 KiB manager; external singleton scene state | Lowest initially | May duplicate scene runtime | Efficient shared world, long-lived state | Engine/tooling overhead |
| Automation | Strict DevAPI, operation polling, scaffold | Ad hoc | Custom instrumentation | ECS inspection helps state only | Usually editor/tooling driven |
| Complexity | High upfront, bounded and centrally tested | Low initially, rises with flow count | Medium | High conceptual coupling | High engine coupling |

Best fit: finite idle/game-shell flows with singleton screens, settings,
upgrades, rank-up modals, shared model/resources, and deterministic automation.

Poor fit: multiple live instances of one route, split-screen, multiplayer,
open-world/additive composition, arbitrary overlay graphs, recoverable network
loading, or rich input bubbling. Use a classic stack for independent per-entry
instances, ECS tags for a persistent simulation, and an engine router only when
the engine owns the required resource, focus, and editor contracts.

Official references:

- [Defold collection proxies](https://defold.com/manuals/collection-proxy/)
- [Unity `LoadSceneAsync`](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SceneManagement.SceneManager.LoadSceneAsync.html)
- [Unity Addressables scene loading](https://docs.unity3d.com/Packages/com.unity.addressables%402.9/manual/LoadingScenes.html)
- [Godot manual scene changes](https://docs.godotengine.org/en/stable/tutorials/scripting/change_scenes_manually.html)
- [Godot background loading](https://docs.godotengine.org/en/stable/tutorials/io/background_loading.html)
- [Unreal CommonUI activatable widget stack](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonActivatableWidgetStack)
- [Unreal asset management](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine)
- [Phaser scene concepts](https://docs.phaser.io/phaser/concepts/scenes)
- [Bevy state module](https://docs.rs/bevy/latest/bevy/state/index.html)

## 3. Decision-by-decision assessment

### Chosen: singleton catalog with repeated history occurrences

This follows the useful property of the local Defold manager and Phaser-style
registered scenes: identity is stable and test tooling has deterministic ids.

Unlike instance-based Unity/Godot navigation, two occurrences do not create two
worlds. That is an accepted constraint. Per-occurrence immutable route arguments
are therefore necessary: revisiting a previous modal/screen must reapply that
entry's configuration to the singleton.

### Chosen: target-first navigation

Waiting for a different target before source teardown is consistent with staged
scene loading across Defold, Unity, Godot, and Phaser. It avoids an empty frame
and establishes target readiness before the atomic presentation/history commit.
It is not a transaction: lifecycle callbacks and transitions may already have
produced external effects, and invariant failures abort rather than roll back.

Reload is the deliberate exception: the user explicitly asks to discard and
recreate the focused runtime, so a loading shell may be visible.

### Chosen: scene-owned readiness

No engine's generic "pack/scene loaded" flag can prove that all game-specific
runtime objects required for the first valid frame exist. The two-step
`load_begin/load_step` callback keeps this knowledge with the scene while the
manager owns timeout and ordering.

### Chosen: small immutable route arguments, not state serialization

World engines generally do not serialize navigation state for ordinary
back-stack behavior. UI/navigation stacks commonly retain route configuration,
while live domain data remains in application state.

The 64-byte inline value follows that separation:

- history owns stable routing configuration;
- the game model owns current item/player/world data;
- returning through history re-runs `on_show(args)` and reads current data.

### Chosen: modal suffix owns whether the covered screen updates

Unreal CommonUI and many UI stacks separate activation/input from underlying
world ticking. The simple PAUSE/CONTINUE modal policy captures the actual game
need without exposing arbitrary layers or lower-scene lists.

For stacked modals, "any PAUSE wins" is deterministic and conservative.
Functional input still belongs only to the top.

### Chosen: no generic input event system yet

Defold messages, Unity input maps, Unreal CommonUI routing, and Phaser input
plugins differ substantially. Neotolis already exposes polled input and Clay
handles widget arbitration. A `can_process_input` gate solves the current
fall-through problem without prematurely designing consumption/bubbling.

### Chosen: preload as a hint

Unity/Addressables handles and Godot resource references support explicit
retention, but Neotolis does not expose the same ownership model. A scene-manager
lease/refcount would therefore create policy without real underlying ownership.

V1 preload should only begin work early. `LOADING`/`READY` makes repeated calls
idempotent. Like the background-loading facilities in the compared engines, it
must not occupy the product-navigation slot. Navigation joins an existing target
load. A successfully preloaded scene that has never been presented remains
READY until it is later shown and hidden or the manager shuts down; there is no
background eviction sweep, so heavy preloads require an explicit memory budget.

### Chosen: packs outside the manager

Defold proxies, Unity Addressables, Godot resources, Unreal Asset Manager, and
Phaser caches all demonstrate that resource residency is its own subsystem.

A dedicated Neotolis pack can be a useful implementation choice for a heavy
level or screen, but one pack per scene is neither universal nor free. The
manager should call scene lifecycle; the scene/host may choose shared or
dedicated resources.

### Chosen: one pending/active navigation intent

Phaser and Bevy apply state changes at fixed safe phases. All navigation calls
therefore copy one intent for the next manager step. A general queue would
introduce ordering, cancellation, and stale-intent semantics without a
demonstrated use case. Independent preload residency is not a navigation intent.

### Chosen: assertive invariant failures

This project ships with assertions and treats unknown core ids, invalid
descriptors, invalid lifecycle results, and load/transition deadline breaches
as programming/build defects. `load_step` has no recoverable failure result.
Safe discovery and DevAPI preflight provide non-crashing checks where input is
external.

## 4. Differences that are intentional

The current manager does not try to match engines that allow:

- multiple independent runtime instances of the same scene;
- arbitrary additive world composition;
- reference-counted asset ownership;
- recoverable download/load failure UI;
- saved/serialized navigation history;
- general input bubbling and consumption;
- arbitrary overlay/layer graphs.

These can be added only after a concrete game requires them. They should not be
latent abstractions in V1.

## 5. Review conclusions applied

Independent comparison/review changed the draft in five concrete ways:

1. navigation is defined by the diff between old and candidate visible
   projections, not a single generic source;
2. preload progresses as background residency and never occupies navigation;
3. synchronous unload has no hidden async core hook; Neotolis supplies its
   post-unmount step through fixed host frame order;
4. UI isolation uses passive scene builders plus a real host-owned gate;
5. parameterized automation uses game-owned typed endpoints rather than a
   generic JSON-to-C memory codec.

These corrections preserve the chosen singleton/history model while matching
the separation of responsibilities seen in the compared systems.

## 6. Architectural position

Keep the feature as:

> A deterministic application-route manager for singleton game scenes, with
> scene-owned lifecycle and host-owned rendering/input/resource systems.

This captures the useful part of the Defold implementation while remaining
compatible with how other engines separate navigation, world composition, and
resource residency.

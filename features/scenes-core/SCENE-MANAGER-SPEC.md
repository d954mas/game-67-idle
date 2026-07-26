# Universal Scene Manager Specification

Status: FINAL IMPLEMENTED CONTRACT, VERIFIED 2026-07-25

Target engine: Neotolis `0.1.0`, revision
`ffab834c72d1c50b5a5a2ea628521bddab252ccc`

Behavioral reference: `game-dig-ore-3D`, revision
`447903883f4ed7a22e25c53cf286b6ee05ef76d0`

This is the implemented contract for the reusable `scenes-core` feature.

## 1. Purpose and ownership

Every game owns its scene catalog, root scene, content, and composition.
Replacing the template root is normal.

`scenes-core` owns:

- stable scene ids and catalog validation;
- bounded navigation history for ordinary and modal scenes;
- scene residency and lifecycle orchestration;
- one pending/active navigation command;
- visible/update/input/UI policy;
- deterministic state and operation queries.

Scene ids match `[a-z_][a-z0-9._-]{0,126}`. Catalog validation, generic
DevAPI requests/responses, and the agent scaffold share this contract, so every
catalog id is representable by the published schema. Generated C identifiers
replace dots and hyphens with underscores.

The game owns:

- singleton scene objects and their mutable state;
- route-argument types and interpretation;
- runtime entities, systems, and UI;
- resources required by each scene;
- transition visuals;
- modal results and game events;
- debug/capture behavior and release composition.

Neotolis continues to own resources, graphics, input, app lifecycle, and
`nt_ui`/Clay. The manager is not an engine scene graph, renderer, resource
manager, input-event router, or serialization system.

## 2. Memory, catalog, and identity

V1 allocates no heap memory.

```c
enum {
    SCENE_MANAGER_MAX_SCENES = 64,
    SCENE_MANAGER_MAX_HISTORY = 128,
    SCENE_ROUTE_ARGS_INLINE_CAPACITY = 64
};
```

`scene_manager_t` is a caller-owned object containing its fixed storage. Scene
descriptors, callback tables, ids, and scene instances are game-owned and must
outlive the manager.

Each id maps to exactly one scene object:

```text
game
inventory
settings
debug.people
```

The same id may occur repeatedly in history, but all occurrences reference that
one object and at most one runtime generation:

```text
history = [game, inventory(item=7), settings, inventory(item=12)]
catalog[inventory] = one scene object
```

Duplicate ids, more than 64 scenes, an invalid descriptor, or a modal root
assert. The catalog is immutable after initialization.

Core mutations assert on unknown ids. Safe discovery has no side effects:

```c
bool scene_manager_has_scene(
    const scene_manager_t *manager,
    const char *scene_id);

const scene_descriptor_t *scene_manager_find_scene(
    const scene_manager_t *manager,
    const char *scene_id); /* NULL when absent */
```

### 2.1 Descriptor

```c
typedef enum scene_kind {
    SCENE_KIND_SCREEN,
    SCENE_KIND_MODAL
} scene_kind_t;

typedef enum scene_modal_update_policy {
    SCENE_MODAL_PAUSE_BELOW,
    SCENE_MODAL_CONTINUE_BELOW
} scene_modal_update_policy_t;

typedef struct scene_descriptor {
    const char *id;
    scene_kind_t kind;
    bool keep_loaded;
    bool debug_only;
    scene_modal_update_policy_t modal_update_policy; /* modal only */
    uint8_t route_args_size;                         /* 0..64 */
    void *instance;
    const scene_api_t *api;
    const scene_transition_api_t *transitions;       /* optional */
} scene_descriptor_t;
```

`keep_loaded=false` means that a scene which becomes hidden through navigation
is unloaded after the new presentation is established. `keep_loaded=true`
keeps its runtime dormant and READY.

`debug_only` is metadata. Release exclusion is structural: source, descriptor,
endpoint, and assets are omitted from release composition.

The modal update-policy default is `SCENE_MODAL_PAUSE_BELOW`.

## 3. Immutable route arguments

Every history occurrence owns a copy of its route arguments:

```c
typedef struct scene_route_args_view {
    const void *data;
    uint8_t size;
} scene_route_args_view_t;
```

Rules:

- `size` must exactly equal the target descriptor's `route_args_size`;
- `data` is NULL iff `size == 0`;
- the manager copies the bytes into its pending command and later history;
- no heap, pointer ownership, destructor, or serialization is involved;
- values are stable ids, enums, numbers, and small inline values;
- large/live data stays in the game model and is referenced by stable id;
- history arguments are immutable;
- returning through history reapplies that occurrence's arguments and reads
  current data from the game model;
- arguments do not change mandatory lifecycle resources. Different mandatory
  asset closures require different scene ids; argument-dependent assets may be
  optional/lazy after READY.

Argument storage is opaque bytes. A scene must `memcpy` the view into a local
typed value. Casting `args.data` to a C struct pointer is forbidden. The view is
valid only for the duration of `on_show`.

No bytewise equality or custom equality callback exists. Therefore:

```text
parameterless show of current top -> ALREADY_TOP
parameterized show of current top -> push a new occurrence
```

Even identical parameter bytes create a new parameterized occurrence.

## 4. Lifecycle and residency

### 4.1 Residency

Residency belongs to the singleton, not to a history entry:

```text
UNLOADED -> LOADING -> READY
    ^                     |
    +---------------------+
```

There is no recoverable FAILED or UNLOADING state.

Each singleton also has a manager-owned `presented` bit recording whether its
successful `on_show` has not yet been paired with `on_hide`. Update, UI, input,
and render-visibility queries require all three:

```text
selected by current history projection && residency == READY && presented
```

This bit is deliberately separate from history so Reload/same-id Replace can
keep the history entry while the old runtime is hidden and the new runtime is
still LOADING.

### 4.2 Callbacks

```c
typedef enum scene_load_result {
    SCENE_LOAD_PENDING,
    SCENE_LOAD_READY
} scene_load_result_t;

typedef enum scene_ui_mode {
    SCENE_UI_INTERACTIVE,
    SCENE_UI_PASSIVE
} scene_ui_mode_t;

typedef struct scene_api {
    void (*load_begin)(void *scene);
    scene_load_result_t (*load_step)(void *scene);
    void (*unload)(void *scene);

    void (*on_show)(void *scene, scene_route_args_view_t args);
    void (*on_hide)(void *scene);
    void (*on_pause)(void *scene);  /* optional */
    void (*on_resume)(void *scene); /* optional */
    void (*on_update)(void *scene, float dt); /* optional */
    void (*on_ui)(
        void *scene,
        void *ui_context,
        scene_ui_mode_t mode); /* optional */
} scene_api_t;
```

The scene knows what it needs:

- `load_begin` starts resource requests and runtime construction;
- `load_step` progresses work and returns READY only when mandatory resources
  and the first valid runtime are ready;
- `unload` synchronously destroys/detaches runtime consumers and releases
  scene-owned resource ownership;
- `unload` must also safely clean a partially LOADING scene during shutdown.

Known resource/runtime failure is logged and asserted by the scene. The manager
asserts illegal lifecycle transitions and a per-scene load deadline. V1 counts
manager steps rather than wall time: the defaults are
`SCENE_MANAGER_LOAD_DEADLINE_STEPS=36000` and
`SCENE_MANAGER_TRANSITION_DEADLINE_STEPS=3600`; build configuration may
override either macro.

`on_show(args)` applies the active history occurrence and activates the scene's
game-owned presentation membership. After it returns, the manager sets
`presented=true`. Immediately before `on_hide()`, the manager sets
`presented=false`; the callback deactivates game-owned presentation membership.
A retained scene must not keep rendering entities merely because its runtime
still exists.

`on_pause/on_resume` report changes in update eligibility for a visible,
staying ordinary scene. `on_draw` is deliberately absent: render systems remain
host-owned.

### 4.3 Loading scheduler and preload

Every LOADING scene is progressed once per `scene_manager_step`, in deterministic
catalog order. Several scenes may be LOADING: background preload is independent
of the one navigation operation.

Preload only schedules early loading:

```c
typedef enum scene_preload_result {
    SCENE_PRELOAD_SCHEDULED,
    SCENE_PRELOAD_ALREADY_SCHEDULED,
    SCENE_PRELOAD_ALREADY_LOADING,
    SCENE_PRELOAD_ALREADY_READY
} scene_preload_result_t;

scene_preload_result_t scene_manager_preload(
    scene_manager_t *manager,
    const char *scene_id);
```

For UNLOADED, the manager sets one per-scene request bit; `load_begin` runs at
the next manager step. LOADING/READY are no-op results. Preload creates no
navigation operation, operation id, hold, lease, use count, cancellation, or
deferred-navigation slot.

If navigation targets an already LOADING scene, it waits for that same load.

There is no background eviction sweep. A scene that preload brought to READY
stays READY until it is later shown and hidden, or manager shutdown. After it
has been shown, ordinary `keep_loaded` behavior applies.

### 4.4 Reload and local state

The manager never snapshots, serializes, or generically resets scene state.

`reload()` uses the ordinary lifecycle:

```text
exit -> on_hide -> unload -> later step -> load_begin/load_step
     -> on_show(existing args) -> enter
```

The scene decides what `unload` or `load_begin` resets. All history occurrences
remain and observe the new runtime generation. Old and new runtime generations
never coexist.

## 5. History projection and presentation diff

### 5.1 Visible projection

History projects to at most two visible singletons:

1. screen on top: only that screen;
2. modal on top: newest ordinary screen below it plus the top modal;
3. older screens and previous modals are hidden;
4. a singleton is dispatched at most once per frame.

Modal over modal is valid, but only the top modal is visible.

### 5.2 Diff model

Every navigation operation first computes candidate history, then:

```text
old_visible
new_visible
leaving      = old visible only
staying      = visible in both with same active occurrence
entering     = new visible only
reactivated  = same singleton visible in both, but focused occurrence changed
recreated    = explicit same-id Replace or Reload of the focused singleton
```

This, rather than a single generic "source scene", defines callbacks.

Examples:

```text
show modal:       {screen A} -> {screen A, modal M}
                  staying={A}, entering={M}

show screen B:    {screen A, modal M} -> {screen B}
                  leaving={M,A}, entering={B}

close modal:      {screen A, modal M} -> {screen A}
                  leaving={M}, staying={A}

same modal args:  {A, item(args2)} -> {A, item(args1)}
                  staying={A}, reactivated={item singleton}
```

### 5.3 Atomic callback order

An operation uses this order:

1. validate and compute candidate history/diff;
2. load every entering singleton before source teardown;
3. if the old focused occurrence is leaving, reactivated, or recreated, run
   only its exit transition;
4. clear `presented` and call `on_hide` for leaving scenes top-to-bottom and for
   reactivated/recreated focus;
5. commit history atomically;
6. for staying scenes, call `on_pause` on eligible→ineligible and `on_resume`
   on ineligible→eligible;
7. call `on_show(args)` for entering scenes bottom-to-top and
   reactivated/recreated focus, then set `presented`;
8. if the new focused occurrence is entering, reactivated, or recreated, run
   only its enter transition;
9. enable input for the established top;
10. unload leaving singletons that are no longer visible and are not retained.

A leaving scene gets `on_hide`, not an artificial `on_resume`. Update/UI
callbacks never run between steps 4-7. `on_hide` observes old committed history;
`on_show/on_pause/on_resume` observe new committed history.

Same-id `replace` and `reload` are exceptions to target-first loading: a
singleton cannot load a second runtime beside itself. They force `recreated`,
exit, clear presentation/hide, unload, yield until a later manager step, then
load/show/present/enter. Reload forces this path even though its history
occurrence and arguments do not change. During the gap, no scene callback or
render iteration can dispatch the recreated singleton; the host loading surface
is shown.

## 6. Update, UI, input, and render

### 6.1 Update

The host calls:

```c
void scene_manager_update(scene_manager_t *manager, float dt);
```

Eligible visible scenes update bottom-to-top. Hidden scenes never update.
Visible scenes continue updating through target loading and enter/exit
transitions.

An ordinary screen under a modal suffix updates only if every modal in that
suffix uses `SCENE_MODAL_CONTINUE_BELOW`. Any PAUSE modal wins.

### 6.2 Input

There is no V1 `on_input`, consumed flag, bubbling, or pass-through system.

```c
bool scene_manager_can_process_input(
    const scene_manager_t *manager,
    const scene_descriptor_t *scene);

bool scene_manager_input_gated(
    const scene_manager_t *manager);
```

Only the established focused top returns true. The ordinary screen under a
modal always returns false, even if it keeps updating.

Every accepted pending or active navigation command gates functional input
immediately. The target receives input only after show and enter complete.

All direct game `nt_input` consumers must use the query. Shell/platform input
consumers must be explicitly classified; shell Escape-to-quit must not run when
the manager owns or gates Escape.

### 6.3 Immediate UI

The host owns `ui_runtime_begin/end`. Between them:

```c
void scene_manager_build_ui(
    scene_manager_t *manager,
    void *ui_context);
```

Visible scene builders run bottom-to-top:

- stable top: INTERACTIVE;
- ordinary screen below a modal: PASSIVE;
- while navigation is pending/active: every scene is PASSIVE.

PASSIVE scenes build their complete visual UI, but every interactive `nt_ui`
widget must be declared disabled/display-only, including text fields, menus,
dropdowns, and keyboard-focus widgets. They must not act on widget results.
On the first passive frame, every previously focused widget that remains visible
must be redeclared disabled; `nt_ui_input(..., enabled=false)` clears its own
focus. Focus owned by a now-hidden scene is cleared by the normal next
`nt_ui_begin` focus reconciliation.
The Neotolis host additionally declares a shell-owned full-screen pointer gate
after all scene/global UI while navigation is gated. The gate prevents pointer
fall-through only. Keyboard, text, and focus isolation come from redeclaring
every visible widget PASSIVE/disabled, omitting hidden/global keyboard UI, and
gating raw input through `scene_manager_can_process_input`.

Global shell UI must be disabled or omitted while navigation is gated. Placing
an enabled keyboard/text widget below the pointer gate is not sufficient.

### 6.4 Rendering

The manager has no draw callback. `on_show/on_hide` must activate/deactivate the
scene's render-system membership, or render systems must filter by manager
visibility. A hidden `keep_loaded` scene cannot remain visually present.

Exceptional debug rendering is called explicitly by the game host after a
presentation query; it is not a portable scene callback.

## 7. Transitions

Transitions are optional:

```c
typedef enum scene_transition_direction {
    SCENE_TRANSITION_ENTER,
    SCENE_TRANSITION_EXIT
} scene_transition_direction_t;

typedef enum scene_transition_result {
    SCENE_TRANSITION_PENDING,
    SCENE_TRANSITION_DONE
} scene_transition_result_t;

typedef struct scene_transition_api {
    void (*begin)(
        void *scene,
        scene_transition_direction_t direction);
    scene_transition_result_t (*step)(void *scene, float dt);
} scene_transition_api_t;
```

`dt` comes from manual/game time supplied to `scene_manager_step`; deadline
accounting itself uses monotonic manager step indices, so paused/manual time
cannot leave a transition permanently pending.

Only the old focused occurrence may own exit and only when
leaving/reactivated/recreated. Only the new focused occurrence may own enter and
only when entering/reactivated/recreated. Other scenes affected by an atomic diff do not run
sequential decorative transitions.

## 8. Navigation commands and operation status

All state-changing navigation APIs validate and copy one intent. The intent
activates only at the next manager step:

```c
void scene_manager_step(
    scene_manager_t *manager,
    uint64_t frame_index,
    float dt);
```

There is one pending-or-active navigation command and no queue. A second command
returns BUSY and identifies the blocking operation id.

```text
PENDING
ACTIVE:
  WAIT_ENTERING_READY
  EXIT_OLD_FOCUS
  UNLOAD_RECREATED_FOCUS
  WAIT_RECREATED_READY
  APPLY_PRESENTATION_DIFF
  ENTER_NEW_FOCUS
  CLEANUP
COMPLETED
```

Normal navigation uses WAIT_ENTERING_READY before EXIT_OLD_FOCUS. A recreated
same-id focus instead uses EXIT_OLD_FOCUS → UNLOAD_RECREATED_FOCUS, yields, then
WAIT_RECREATED_READY before applying the new/current entry.

Each manager step has a fixed order:

1. activate the pending navigation command and mark its required loads;
2. convert preload request bits into LOADING scenes;
3. call `load_step` once for every LOADING scene in catalog order;
4. advance through consecutive synchronous phases until the operation reaches
   an asynchronous wait, the explicit reload yield, or completion.

Accepted commands receive a monotonically increasing operation id immediately.
Ids are capped at `SCENE_OPERATION_ID_MAX` (`2^53 - 1`) so every core id remains
exactly representable in the JSON/JavaScript DevAPI contract.
Status retains the pending/active operation and exactly the last completed
operation. Older ids return OPERATION_NOT_FOUND.

Only `on_update/on_ui`, host code, and DevAPI may request navigation.
Navigation/preload mutation from `load_begin/load_step/unload`, `on_show`,
`on_hide`, `on_pause/on_resume`, or transition callbacks asserts. This prevents
reentrant lifecycle mutation. Calling `scene_manager_update`,
`scene_manager_build_ui`, `scene_manager_step`, or shutdown from those
callbacks also asserts; lifecycle dispatch cannot nest consumer dispatch.

Immediate results:

```text
ACCEPTED
BUSY
ALREADY_TOP
NOT_TOP
ROOT_PROTECTED
```

`NOT_TOP` is available to game-owned id-specific dismissal helpers; the generic
core navigation functions do not currently produce it. Unknown ids and illegal
calls assert in core. DevAPI preflights them.

## 9. Navigation semantics

All target-taking commands copy an argument view of the descriptor's exact
size.

### 9.1 Start

Starts an empty manager with one ordinary scene. A modal first or a second start
asserts.

### 9.2 Show

- modal: push over an existing route;
- screen: atomically remove the modal suffix, then push the screen;
- parameterless screen equal to the revealed ordinary top only closes the modal
  suffix and does not create an adjacent duplicate;
- parameterless current top returns ALREADY_TOP;
- parameterized current top pushes a new occurrence;
- different entering scenes load before old focused exit.

### 9.3 Replace

Replaces the top entry without presenting an intermediate route. Source and
target kind must match.

Replacing with the same id performs a full unload/load and replaces the entry's
arguments. It may show the host loading surface.

### 9.4 Back

```c
scene_result_t scene_manager_back(
    scene_manager_t *manager,
    size_t count,
    scene_operation_id_t *operation_id);
```

`count > 0`. It removes exactly that many entries atomically and never presents
intermediate entries. Removing the root returns ROOT_PROTECTED. An unloaded
destination loads before old focused exit.

### 9.5 Back to id

`back_to(id)` searches strictly below the current top and selects the nearest
previous occurrence. It removes everything above it atomically. Missing target
below asserts.

```c
bool scene_manager_can_back_to(
    const scene_manager_t *manager,
    const char *scene_id);
```

The safe query returns false for unknown/absent-below ids. There is no
ALREADY_TOP result for Back to id.

### 9.6 Close modals

Removes the full modal suffix atomically. Hidden intermediate modals are never
shown or transitioned. With no modal suffix it returns ALREADY_TOP.

### 9.7 Reload

Retains the current history entry and arguments, but performs the normal full
unload/load lifecycle. It may show the host loading surface.

## 10. Resources and packs

The manager knows nothing about `.ntpack` files.

Typical scene:

- requests resources from the host's already-mounted shared pack;
- destroys its runtime on unload;
- cannot individually evict shared textures/models because Neotolis exposes no
  per-resource release/refcount API.

Optional heavy level/screen:

- mounts a dedicated pack in `load_begin`;
- unmounts it synchronously in `unload`.

A pack contains and registers all resources built into it. Separate packs are
the available unit when independent pack residency is required; one pack per
scene is not the default.

Core has no resource-step serial or unload deadline. It never calls
`load_begin` in the same `scene_manager_step` after unloading that singleton.
The Neotolis host order guarantees `nt_resource_step` before the next manager
step, which supplies the real post-unmount publication barrier for reload.

During shutdown, the host unloads scenes and then executes the required
`nt_resource_step` before `nt_resource_shutdown`.

Selective activation/unload inside one pack remains a separate engine issue.

## 11. Assertions and shutdown

Core asserts:

- unknown mutation id;
- duplicate/invalid descriptor or capacity overflow;
- route-argument size/data mismatch;
- modal root or illegal kind operation;
- history overflow;
- missing Back-to target;
- lifecycle/transition reentrancy or illegal state;
- load or transition deadline.

Scene/resource code logs resource detail before its own assert. In debug builds,
the default core assertion identifies the violated expression and aborts. In
`NDEBUG` builds it remains an unconditional abort without diagnostic formatting.
The core deliberately does not pull formatting/logging into the runtime. A
consumer that needs scene id, operation id, or phase telemetry records it before
invoking the core operation or supplies its own debug assertion integration.

Production has no assertion recovery, rollback, retry, or error-scene system.
There is no assertion recovery mechanism. Debug/assert-enabled builds fail
loudly at the call site.

Shutdown is the only operation allowed to abandon pending/active work. It does
not wait for navigation or transitions to finish. It:

1. blocks new mutation and consumer dispatch;
2. discards the pending/active command and clears unstarted preload bits;
3. calls `on_hide` for each still-presented scene that was not already hidden;
4. calls `unload` exactly once for every LOADING or READY singleton, regardless
   of the operation phase;
5. clears history and leaves all scenes UNLOADED;
6. lets the host perform its final resource step and subsystem shutdown.

## 12. Automation

Generic methods:

```text
game.scene.list
game.scene.status
game.scene.operation_status
game.scene.preload
game.scene.show
game.scene.replace
game.scene.reload
game.scene.back
game.scene.back_to
game.scene.close_modals
```

The Neotolis transport owns the request/response envelope. Scene handlers
strictly validate their `params` object and reject unknown/duplicate keys,
wrong types, non-integral numbers, ranges, and unknown ids before core mutation.

Generic `show/replace` support only parameterless scenes. A parameterized scene
uses a game-owned typed endpoint. The generic adapter returns
`typed_endpoint_required`; the game documents and registers the concrete
endpoint. That endpoint validates JSON, constructs a typed local struct, and
calls the binary core API. Core has no JSON codec registry or
machine-readable JSON-to-memory system.

Generic history status exposes scene id and argument byte size, not raw bytes or
decoded values. A typed scene endpoint may expose its domain status separately.

Navigation responses return operation id; agents poll operation status. Status
distinguishes PENDING/ACTIVE/COMPLETED and retains only the last completed id.
BUSY identifies the blocking operation.

Preload returns scheduled/already-scheduled/already-loading/already-ready with
no operation id. Agents poll scene residency.

Capture scenes use the same lifecycle, but capture parameters, actions,
readiness, semantic hashes, and endpoint schemas remain game/runtime-automation
extensions rather than `scenes-core` types.

## 13. Scene scaffold

The V1 generator creates:

- scene `.h/.c` with lifecycle/callback stubs;
- descriptor and game-owned catalog registration;
- optional `debug_only` source/catalog composition.

It does not create a pack or dedicated-pack flag. Dedicated resource ownership
is a manual advanced recipe. The generated scene is intentionally
parameterless; route-argument types, typed DevAPI endpoints, and scene-specific
tests are explicit game-owned follow-up work.

Generator mutation is dry-run/checkable, validates the expected catalog/CMake
anchors and existing state before writing, and rolls back every destination on
a handled write/rename failure.

## 14. Acceptance

The required suites prove:

1. fixed-capacity, heap-free catalog/history and singleton identity;
2. 0..64-byte route args, NULL rules, memcpy-only typed use, and callback
   lifetime;
3. repeated/identical parameterized entries restore historical args without a
   second runtime, including `[A(args1), B, A(args2)] -> back(2)`;
4. background preload schedules once, never occupies navigation, attaches to
   Show, creates no hold, and is not immediately swept;
5. each LOADING scene steps once per manager step in catalog order;
6. old/new presentation diff for screen↔screen, screen↔modal, stacked modal,
   same-singleton reactivation, atomic Back, and close-all;
7. exact callback/commit ordering and query observations;
8. pause/resume edges derive only from staying update eligibility;
9. any PAUSE modal stops ordinary update; all CONTINUE allows it;
10. accepted pending navigation immediately disables raw input and makes all UI
    passive;
11. the host gate prevents source/global UI click-through during target load,
    enter, and exit;
12. hidden `keep_loaded` entities do not render;
13. target loads before teardown except same-id replace/reload;
14. same-id replace/reload yield a manager step after unload before remount/load;
15. screen and modal Reload force recreated lifecycle, dispatch nothing during
    the loading gap, and preserve history arguments;
16. only focused leaving/reactivated/recreated and
    entering/reactivated/recreated transitions run;
17. mutation from lifecycle/transition callbacks asserts; update/UI gets the one
    pending command;
18. pending/active/last-completed operation status and BUSY blocker are
    deterministic;
19. shared-pack unload destroys runtime without claiming asset eviction;
20. optional dedicated-pack reload observes the host resource step boundary;
21. shutdown from every operation phase plus LOADING preload, hidden retained,
    and visible scenes unloads each singleton exactly once;
22. strict generic DevAPI params and parameterized-route rejection happen
    before mutation;
23. debug source/catalog/endpoint/assets are structurally absent from release;
24. generator dry-run/check/idempotency and disposable scene E2E pass;
25. native/web template and a second consumer pass navigation/shutdown smoke
    tests.

Evidence ownership is explicit:

- `test_scene_manager_catalog`: item 1 catalog/capacity behavior;
- `test_scene_manager_navigation`: items 2-4, 6, 13-14, and 18;
- `test_scene_manager_ordering`: items 6-7 and 16, including callback-time
  history observations and skipped intermediate transitions;
- `test_scene_manager_presentation`: items 8-10 and 15, including mixed modal
  policies and modal reload arguments;
- `test_scene_manager_lifecycle` and `test_scene_manager_deadlines`: items 4-5,
  19-21, deadlines, and shutdown phases;
- `test_scene_manager_reentrancy`: item 17 and every forbidden lifecycle,
  transition, update, and UI recursion boundary;
- `test_scene_manager_devapi` plus `test_scene_devapi_schema.mjs`: items 18 and
  22;
- `test_consumer_scene_contracts.mjs`: allocation-free core, raw-input/render
  gates, pointer-gate ordering, debug composition, resource-step barriers, and
  public template ownership for items 1, 10-12, 19-20, and 23;
- `test_scaffold_scene.mjs`: item 24, including generated-code compilation and
  transactional rollback;
- `test_scene_manager_web_smoke`: the public template half of item 25. The
  selected real second consumer owns and runs its host-lifecycle suite in its
  private repository; concrete private identity and paths never enter this
  public contract.

Every public acceptance requirement has an owning automated check. Item 25 is
complete only when the selected private consumer's host-lifecycle suite also
passes; the public repository does not claim or duplicate that private result.

## 15. Locked decisions

1. singleton scene per id; repeated history occurrences are valid;
2. immutable inline route arguments up to 64 bytes; no snapshots;
3. scene lifecycle is `load_begin/load_step/unload`;
4. READY includes mandatory resources and first valid runtime;
5. reload is ordinary lifecycle with scene-owned state reset;
6. history operations use old/new presentation diff;
7. preload is an independent no-hold background hint;
8. hidden scenes unload by default; `keep_loaded` retains dormant runtime;
9. manager has no pack knowledge; dedicated packs are optional;
10. only top modal is visible;
11. modal PAUSE/CONTINUE controls lower-screen update; any PAUSE wins;
12. no generic input event/consume/bubble system;
13. immediate UI has interactive/passive scene modes plus a real host gate;
14. no draw callback; presentation membership belongs to scene/render systems;
15. all navigation commands activate at the next step;
16. one pending/active navigation command; no queue;
17. `back(count)` and nearest previous `back_to(id)` are required;
18. same-id Replace is a full lifecycle with new args;
19. modal results and capture semantics remain game-owned;
20. unknown core ids and lifecycle failures assert loudly;
21. fixed caller-owned V1 storage; no heap;
22. scaffold creates parameterless code/catalog/build wiring, never a pack by
    default.

# Scene Manager Plan Review

Status: FINAL 0.3.0 REVIEW; ALL CONFIRMED FINDINGS FIXED 2026-07-25

Review date: 2026-07-24

## Scope

Six independent read-only reviewers evaluated:

1. navigation/lifecycle state-machine correctness;
2. V1 minimalism and API surface;
3. actual Neotolis/template integration;
4. DevAPI schema and AI-agent ergonomics;
5. implementation-plan executability and feature packaging;
6. comparison with Defold, Unity, Godot, Unreal, Phaser, and Bevy.

No reviewer edited the artifacts. The lead integrated or explicitly rejected
their recommendations.

## Initial verdict

The first round was NO_GO. The core direction was accepted, but implementation
would have required inventing behavior in several places.

| Finding | Resolution |
|---|---|
| Operations described one generic source, but screen/modal changes affect a visible set | Navigation now computes candidate history and an old/new presentation diff with leaving/staying/entering/reactivated/recreated classifications |
| Preload could occupy the only navigation operation | Preload is independent background residency with per-scene request bits, no operation id, hold, or navigation slot |
| `void unload()` promised an undefined async barrier/deadline | Unload is synchronous; core yields before same-scene reload and the fixed Neotolis host order supplies the next resource step |
| UI enum alone could not prevent click-through | V1 has INTERACTIVE/PASSIVE; passive widgets are disabled, focus is cleared, and a real host-owned top gate blocks scene/global UI |
| Manager/history allocation and route-args alignment were undefined | V1 uses fixed caller-owned storage; route bytes are opaque and must be copied with `memcpy`, never cast |
| Reload did not differ from an unchanged staying occurrence | Reload forces the recreated lifecycle while preserving history/args |
| History still looked visible during reload's loading gap | A manager-owned presented bit gates every update/UI/input/render query |
| Lifecycle/transition mutation was reentrant | All navigation is one intent for the next step; lifecycle/transition mutation asserts |
| Transition ownership for multi-scene diffs was ambiguous | Only old focused leaving/reactivated/recreated exits; only new focused entering/reactivated/recreated enters |
| Transition polling depended on global time | Transition step receives host/manual `dt` |
| Generic JSON-to-C route codec was unsafe and overbuilt | Generic endpoints support parameterless scenes; parameterized scenes advertise game-owned typed endpoints |
| DevAPI schema modeled a stricter envelope than Neotolis implements | Envelope is transport-owned/optional; handlers strictly validate only their params |
| Capture protocol leaked into core | Capture schema/capability types were removed from `scenes-core` |
| Automation followed scaffold | Automation is Phase 6; scaffold consumes it in Phase 7 |
| Generator created optional packs | No pack flag/generation; dedicated packs are a manual advanced recipe |
| Shutdown required idle navigation | Shutdown is allowed in every phase, abandons work, hides presentation, and unloads each LOADING/READY singleton once |
| Feature folder broke repository contracts | README, INSTALL, planned feature metadata, index, dependency seed, and validation gate were added |

## Deliberate choices after criticism

- Background preload was chosen over serialized preload because the accepted
  product meaning is a non-blocking hint.
- Several simultaneous LOADING scenes are allowed and stepped deterministically;
  navigation joins the target's existing load.
- Completed operation retention is intentionally only the last completed
  operation, not a journal.
- Generic history status exposes args size, not raw/decoded bytes.
- Same-id parameterized Show always creates a new occurrence; V1 has no
  padding-sensitive equality.
- No input bubbling/consume system, unload state machine, resource serial API,
  navigation queue, pack abstraction, or multiple scene instances were added.

## Follow-up verdicts

After corrections, four focused read-only follow-ups returned FINAL GO:

| Review | Final result |
|---|---|
| Navigation/lifecycle architecture | GO |
| Neotolis resource/UI/input/shutdown integration | GO |
| Automation and DevAPI schema | GO |
| Implementation phase dependencies and gates | GO |

The minimalism and competitor reviews required no separate repeat after their
findings were incorporated: they changed ambiguities and excess mechanisms, not
the accepted product model.

## Post-implementation review and verification

Four focused implementation reviewers checked architecture, state-machine
behavior, consumer/build integration, and automation/tests. Their confirmed
findings were fixed:

- same-singleton screen/modal reactivation now pairs hide/show and preserves
  exit/enter transitions;
- consumer callbacks may enqueue navigation but cannot recursively drive or
  shut down the manager;
- both consumers gate world rendering and simulation through scene
  presentation, including the reload gap;
- trolley scene shutdown precedes feature/platform teardown;
- scaffold staging rollback tracks temporary paths before writes;
- deadline tests identify the intended assertion;
- schema/scaffold suites are registered with CTest.
- trolley testbed/capture updates are dispatched only by a presented screen;
- overlay `is_open` queries use presentation rather than buried history;
- native and Emscripten CTest both run scene lifecycle/tooling smoke tests.

Acceptance now also exercises the 64-scene and 128-history bounds, 64-byte and
invalid route arguments, repeated parameterized occurrences, stacked modals,
close-all/back-to, multiple shutdown phases, strict DevAPI type/range/key
validation, compilation of generated scene code, and native/WebAssembly
consumer builds.

Current scoped evidence:

```text
template full devapi-debug CTest: 42 passed, 0 failed
template native scene/DevAPI/tooling subset: 10 passed, 0 failed
template WebAssembly scene smoke/tooling CTest: 3 passed, 0 failed
trolley host/consumer/tooling CTest: 3 passed, 0 failed
feature contracts: 7 modules, 2 pointers; 8 tests passed
template game: devapi-debug, native-release, and WebAssembly-release build
```

## Final 0.3.0 remediation

The final adversarial review found two reachable defects and two contract
misstatements. Version 0.3.0 closes them:

- lifecycle/transition callbacks can no longer nest update or UI dispatch;
- the template platform gameplay signal follows focused-scene input ownership,
  not settings visibility;
- id-specific settings dismissal reports `NOT_TOP` instead of inventing a BUSY
  operation;
- assertion and pointer-gate documentation now state the exact runtime
  behavior rather than promising diagnostics or keyboard blocking the core
  does not implement.

Permanent regressions now cover callback-time history observations, exact
presentation ordering, mixed PAUSE/CONTINUE modal stacks, modal reload argument
retention and dispatch gaps, hidden intermediate transition suppression,
operation-id exhaustion, allocation-free storage, resource/shutdown barriers,
and both consumer integrations. The acceptance section in
`SCENE-MANAGER-SPEC.md` maps every requirement to an owning automated suite.

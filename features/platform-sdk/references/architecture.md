# Platform integration boundaries

The game consumes the C platform facade. The selected backend translates
portal calls and outcomes; it does not own rewards, save conflict decisions,
or the game's choice to enter gameplay.

## Ownership

| Concern | Owner |
| --- | --- |
| Target identity, selected adapter and static policy | `publish-targets/targets.json` |
| Current readiness, operation identity and effective portal/ad pause | C platform facade |
| Browser SDK calls and SDK callback translation | Selected JavaScript adapter |
| Transport read/write state and completion identity | `src/platform_sdk_storage.c` |
| Browser storage ABI | `src/platform_sdk_cloud.c` |
| Save reconciliation and acknowledged snapshots | `features/game-state` save sync coordinator |
| Save retries, base persistence and transactional adoption | Optional `game-state/game_save_cloud` runtime |
| Transport binding | Consumer `sys_cloud_save` adapter |
| Schema validation, startup gate and world/UI rebind | Game composition |
| Progress comparison and ambiguous-conflict presentation | Game-owned save policy and UI |
| Release compilation profile and artifact agreement | Template build and package tools |

## Ad operations and lifecycle

Async backends capture `platform_sdk_active_interstitial_request_id()` or
`platform_sdk_active_rewarded_request_id()` when dispatching. Their completion
uses the matching `platform_sdk_backend_complete_*_request(id, result)`.
The legacy completion functions are valid only during a synchronous backend
call. A result from an older operation cannot settle a newer request or grant
another reward.

Portal pause, pending ad requests, and visible ad overlays contribute separate
pause reasons. Early portal pause/audio events are retained until WASM installs
its bridge. The game resumes only when the last effective pause reason clears.
A watchdog timeout settles the operation without proving that the SDK closed
its overlay; late visibility remains correlated until a definitive close.

## Storage contract

Backend `loadData(key)` resolves one of `{ status: "found", value }`,
`{ status: "missing" }`, `{ status: "unavailable" }`, or
`{ status: "failed" }`. `saveData(key, value)` resolves
`{ status: "acknowledged" }`, `{ status: "unavailable" }`, or
`{ status: "failed" }`. A rejected promise is also a failure. A missing key
is the only successful read with no value; exceptions never become absence.

The C transport owns one replaceable read and one write in flight. Each
completion echoes the request id supplied to the backend. Resetting or
replacing a backend invalidates pending completions. The transport owns no
autosave interval, retry queue, save format, or conflict policy. Native
consumers can inject `platform_sdk_cloud_backend_t` without JavaScript.
Facade and save-runtime calls belong to the game thread. Native backends
marshal asynchronous completions back to that thread before calling the C
completion API; the facade and coordinator do not own a worker scheduler.

The storage capability is bounded by target policy. A backend implementing
no-op method stubs is not sufficient to enable storage on Poki or itch.

Acknowledgment has the durability offered by the portal API. Yandex uses
`setData(data, true)`: its default mode acknowledges only a queued value,
not transmission. See the [Yandex player-data contract](https://yandex.com/dev/games/doc/en/sdk/sdk-player).
CrazyGames accepts synchronous `data.setItem`; the SDK owns subsequent
account synchronization, and guests use device storage. This wrapper cannot
invent a server acknowledgment absent from that API. See the
[CrazyGames data contract](https://docs.crazygames.com/sdk/data/).

## Save synchronization

The game-state coordinator compares the current local document, remote
document, and last shared base document. Wall-clock timestamps do not decide
which divergent branch wins. Unknown or failed reads prohibit uploads;
startup may proceed locally while that decision remains unresolved.

Only an acknowledged write advances the base, and only to the exact snapshot
sent by that write. Later local changes remain pending. A failed write is
reconciled again before retrying. An automatic remote choice is deferred to a
frame boundary and checked against the current live state again. If gameplay
changed its feature state after the decision, the replacement is cancelled.
The game policy is also evaluated again so a playtime tie-break cannot apply
a decision whose winner has changed.

The shared game-state runtime validates through the game's registered save
validator before replacing its local slot and persists the shared base locally.
The game calls adoption at its safe point and rebinds its own runtime consumers. Legacy installations without a shared base
preserve divergent local and cloud documents as a conflict instead of using
device clocks to overwrite either branch.

A game-owned policy gets the first opportunity to resolve a conflict using
its own progression semantics. A clear winner is selected automatically;
unsupported, equal, or contradictory progress falls back to the Settings
choice. The SDK does not interpret currency balances, wall clocks, or content
completion. The template compares hero level and tutorial completion. At equal
milestones, two positive recorded playtimes can break the tie; contradictory
milestones still require a choice. Both documents must pass the staged whole-save
validator before an automatic decision. Legacy missing playtime is unknown for
this tie-break, even though the counter itself loads it as zero.

Keeping this device pins the
observed remote version and re-reads it before uploading. Choosing cloud
validates and persists that document at the end of the UI frame, reloads the
registered state fragments, and resets transient world state. A missing remote
document offers only the local choice. A hung read is replaced after the
wrapper's monotonic retry interval; old request completions remain invalid.

Portal stores do not expose a universal compare-and-swap operation. Reading
before writing detects observed divergence but is not an atomic multi-device
transaction; SDK caches and simultaneous writes can still limit detection.
Games requiring that guarantee need a backend with conditional revisions.

## Migration from SDK 1.x

SDK 2.0 changes the internal JavaScript storage return contract and separates
transport from save synchronization. Custom adapters must return the explicit
outcomes above; returning `null` or swallowing write failures is insufficient.
Compile `platform_sdk_storage.c` alongside `platform_sdk_cloud.c`. Native
consumers inject the storage backend separately from the main SDK backend.

Game consumers that used the old template cloud mirroring should adopt the
optional `game_save_sync` coordinator and the updated `sys_cloud_save` wrapper.
The existing cloud envelope remains readable. A custom async ad backend must
capture the operation id and migrate to the request-correlated completion API.

## Build identity

Target descriptors feed CMake and JavaScript tooling. A release must agree on
target, adapter and compile profile across its runtime record, HTML/JS and
compiled WASM witness. A source-only fingerprint is insufficient because the
same sources can be compiled for different portals.

An artifact directory must exist and contain its required files before any
portal acceptance check can pass. Local development is not a portal target.

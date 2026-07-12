# Platform SDK Wrapper Contract

This contract defines one game-facing wrapper over several platform SDK
adapters.

The wrapper has two separate axes:

- **target platform**: where the build runs or is published;
- **platform SDK**: which SDK adapter is active for that target.

`mock` is a platform SDK adapter. It is not a target. `local` and `itch` are
targets that use the `mock` SDK adapter.

## Target And SDK Matrix

| Target | Platform SDK | Notes |
| --- | --- | --- |
| `local` | `mock` | Local development and automation; no network required. |
| `itch` | `mock` | itch.io has no mandatory game SDK; SDK calls become production-safe no-ops. |
| `poki` | `poki` | Direct Poki SDK adapter. |
| `yandex` | `yandex` | Direct Yandex Games SDK adapter. |
| `playgama` | `playgama` | Direct Playgama Bridge adapter. |

## Game-Facing API

The authoritative game-facing API is C. Game code should call only
`features/platform_sdk/platform_sdk.h`; it must not call `window.PokiSDK`,
`YaGames`, `bridge`, or platform globals directly.

JavaScript is a selected web backend only. It loads/calls the chosen portal SDK
adapter and returns SDK outcomes; it does not own gameplay guards, first-input
state, event emission, reward policy, or pause/resume dedupe.

```c
typedef enum platform_target_t {
  PLATFORM_TARGET_LOCAL,
  PLATFORM_TARGET_ITCH,
  PLATFORM_TARGET_POKI,
  PLATFORM_TARGET_YANDEX,
  PLATFORM_TARGET_PLAYGAMA
} platform_target_t;

typedef enum platform_sdk_t {
  PLATFORM_SDK_MOCK,
  PLATFORM_SDK_POKI,
  PLATFORM_SDK_YANDEX,
  PLATFORM_SDK_PLAYGAMA
} platform_sdk_t;

platform_target_t platform_sdk_target(void);
platform_sdk_t platform_sdk_current(void);
bool platform_sdk_external_links_allowed(void);
bool platform_sdk_ads_supported(void);
bool platform_sdk_rewarded_supported(void);
bool platform_sdk_storage_supported(void);

platform_sdk_result_t platform_sdk_init(void);
platform_sdk_boot_status_t platform_sdk_status(void);
platform_sdk_result_t platform_sdk_game_loading_progress(float progress01);
platform_sdk_result_t platform_sdk_game_loading_finished(void);
platform_sdk_result_t platform_sdk_game_ready(void);

void platform_sdk_mark_input(void);
bool platform_sdk_has_input(void);
bool platform_sdk_has_gameplay_started(void);
bool platform_sdk_gameplay_active(void);
platform_sdk_gameplay_start_result_t platform_sdk_gameplay_start(void);
platform_sdk_gameplay_stop_result_t platform_sdk_gameplay_stop(void);

platform_sdk_listener_id_t platform_sdk_on_pause(
  platform_sdk_lifecycle_callback_t callback,
  void *userdata);
platform_sdk_listener_id_t platform_sdk_on_resume(
  platform_sdk_lifecycle_callback_t callback,
  void *userdata);
void platform_sdk_remove_listener(platform_sdk_listener_id_t listener_id);

platform_sdk_result_t platform_sdk_show_interstitial(
  const char *placement,
  platform_sdk_ad_callback_t callback,
  void *userdata);
platform_sdk_result_t platform_sdk_show_rewarded(
  const char *placement,
  platform_sdk_rewarded_callback_t callback,
  void *userdata);
```

Async C callbacks use the standard `callback + userdata` shape. `userdata` is
opaque caller-owned context, not a platform account/player user.

The platform-specific backend is installed through `platform_sdk_set_backend()`.
Web builds call `platform_sdk_install_web_backend()` so the C facade can invoke
the selected JavaScript backend; mobile or desktop builds can install a native
backend with the same semantics.

Use the runtime capability for gameplay/UI decisions. For example, a Telegram
link should be visible when `externalLinksAllowed` is true (`local`, `itch`),
and hidden when the target policy forbids external links (`poki`, `yandex`, and
any other restricted portal). Use `platform_sdk_target()` only when the behavior
is truly target-specific and cannot be expressed as a capability.

`platform_sdk_gameplay_start()` must not reach the selected platform backend
before the first real player input. The game/engine calls
`platform_sdk_mark_input()` when a trusted input edge is consumed. Until
`platform_sdk_has_input()` is true, `platform_sdk_gameplay_start()` returns
`started=false` with `PLATFORM_SDK_RESULT_WAITING_FOR_INPUT`, may warn in
debug/dev builds, and does not call the backend.

First input is only a precondition, not proof that gameplay is active. The game
owns the active-gameplay state machine:

- `platform_sdk_game_ready()` may run when the first interactive menu is
  visible.
- `platform_sdk_gameplay_start()` is called only after first input and when the
  player enters an active gameplay state.
- `platform_sdk_gameplay_stop()` is called when gameplay is interrupted or left, including
  settings, menus, shops, dialogs, game over screens, and ad opportunities.
- Returning from an ad or modal may call `platform_sdk_gameplay_start()` again
  only if the game is returning to active gameplay.

After the first successful start, `hasGameplayStarted()` stays true;
`gameplayActive` tracks the current start/stop interval.

## Build Inclusion Rule

Only the selected SDK adapter may be included in a release build.

Required build inputs:

```text
target platform: local | itch | poki | yandex | playgama
platform SDK:    mock  | mock | poki | yandex | playgama
```

The target manifest computes the SDK adapter from the target. The game build
must not import or link every adapter and decide at runtime, because that ships
unused SDK URLs, policy code, and portal branches.

The template CMake entry is `GAME_PUBLISH_TARGET`; `tools/build_web.mjs` passes
it through and writes target-specific build directories:

```powershell
node tools/build_web.mjs --preset wasm-release --target poki
```

`local` uses `build/wasm-release/bin`; portal targets use
`build/wasm-release-<target>/bin`.

For Emscripten builds, the template uses a checkout-local cache by default:
`build/emscripten-cache`. A caller may override it with `EM_CACHE` or
`GAME_EMSCRIPTEN_CACHE_DIR`, but release verification should not depend on the
global emsdk cache because interrupted links can leave portal builds waiting on
global symbol-list locks.

Implementation rules:

- Keep platform target and platform SDK in one compile-time config module or
  build define.
- The C wrapper facade may be common, but only the selected adapter is imported,
  compiled, or linked.
- Adapter modules must have no side effects at import time. SDK script loading
  happens only after the selected adapter is constructed.
- Mock test helpers are included only in `local`, `itch`, debug, or test builds.
- Template debug UI is C/Clay UI in the consuming template/game, controlled by
  `GAME_PLATFORM_SDK_DEBUG_UI`, and is off in `Release` by default. It must not
  be staged as a web JavaScript artifact.
- Release validation must grep or inspect the built artifact for forbidden SDK
  URLs. Example: an `itch` build must not contain Poki, Yandex, or Playgama SDK
  URLs.
- Runtime identity still exists, but it reports the build-selected target/SDK;
  it is not the mechanism that keeps unused adapters out of the bundle.

## Required Semantics

- `platform_sdk_init()` is the one-shot boot barrier, not a readiness getter. It
  initializes the selected backend; if the SDK is absent, blocked, or
  unavailable, it moves to `PLATFORM_SDK_BOOT_FAILED` because the selected
  portal build cannot run correctly. For web portal SDKs, init may be async:
  `platform_sdk_init()` returns `PLATFORM_SDK_RESULT_NOT_READY` while the
  selected backend is still loading, and the backend completes the C facade with
  `platform_sdk_backend_complete_init()`. JavaScript may cache portal script
  loading inside the selected backend, but that cache is not a game-facing
  policy API.
- `platform_sdk_game_loading_progress(progress01)` is a monotonic, clamped
  `0..1` loading signal. It is allowed before SDK init has completed so the web
  shell can show boot progress while the selected portal SDK and game runtime
  load in parallel. It may forward to portal APIs such as Poki
  `gameLoadingProgress`, but it is not a game event and must not emit
  `game.loading_progress`.
- `platform_sdk_game_loading_finished()` is one-shot. It emits
  `game.loading_finished` through `features/game-events`, and calls the selected
  backend's loading-finished hook at most once per wrapper instance. Web games
  should call it only after the initial required assets are ready and at least
  one playable frame has been presented behind the loading overlay; do not hide
  the HTML loading screen directly from `onRuntimeInitialized`.
- `platform_sdk_game_ready()` is also one-shot and is only for portals with a
  distinct game-ready SDK call, such as Playgama `game_ready`; it does not emit
  an analytics event.
- Unsupported operations return `{ supported: false, shown: false,
  reason: "unsupported" }` or a no-op promise.
- Ad methods fire registered pause callbacks before the ad can cover gameplay
  and resume callbacks exactly once when control returns, including failures.
- Rewarded ads grant rewards only when the SDK confirms reward completion:
  Poki returns a success boolean, Yandex uses `onRewarded`, Playgama uses the
  `rewarded` state.
- Analytics subscribes to the event stream. Do not emit events merely to say
  that analytics forwarding happened. If an adapter forwards selected game
  events to a portal API such as Poki `measure`, that forwarding is an adapter
  sink detail, not a new `track`/`measure` event. `track()`/`measure()` are not
  public game-facing event APIs. The separate internal
  `platform_sdk_measure.h` header exists only for a typed-event bridge; it
  accepts three non-empty lowercase stable tokens of at most 32 characters.
- `local` and `itch` targets must work without network access through the
  `mock` SDK adapter.
- Platform adapter code may dynamically load SDK scripts, but script loading is
  owned by adapters, not by template HTML injection.
- `platform_sdk_external_links_allowed()` is the UI gate for
  creator/community links. Do not scatter portal-name checks through game UI
  code.

## Platform SDK Event Bridge

The C platform SDK facade should emit only events that represent SDK lifecycle
calls or ad flows. Runtime callbacks such as pause/resume/audio/visibility are
API signals to the game, not analytics events by default.

Event payloads stay semantic and compact. Do not duplicate runtime identity
fields such as `target`, `platformSdk`, or `source` into every event; the game
can read them through `platform_sdk_target()`, `platform_sdk_current()`, and
`platform_sdk_capabilities()`, and analytics exports may store them once in
session/header context. Timestamps are also event-spine/export metadata, not
platform-sdk payload fields.

| Event | Payload | When |
| --- | --- | --- |
| `platform.ready` | `ready: boolean` | SDK init success or failure is known. |
| `game.loading_finished` | none | One-shot initial loading lifecycle: the initial required asset set is loaded and the first interactive screen can replace the loading screen. |
| `gameplay.start` | none | Player enters active gameplay after first input. |
| `gameplay.stop` | none | Gameplay pauses, ends, menu opens, or an ad is planned. |
| `ad.interstitial.request` | `placement?: string` | Game asks for an interstitial opportunity. |
| `ad.interstitial.result` | `placement?: string`, `supported: boolean`, `shown: boolean`, `reason?: string` | Interstitial shown, skipped, blocked, or failed. |
| `ad.rewarded.request` | `placement?: string` | Player opts into a rewarded ad. |
| `ad.rewarded.result` | `placement?: string`, `supported: boolean`, `shown: boolean`, `rewarded: boolean`, `reason?: string` | Rewarded ad result; reward grants only when `rewarded` is true. |

`placement` values must be finite, stable IDs such as `revive`,
`double_reward`, or `level_break`. Do not pass dynamic strings, counters, player
IDs, prices, or free-form text.

Do not add platform-sdk events for pure queries or cleanup:

- `platform.capabilities` is not an event. Capabilities are state returned by
  `platform_sdk_capabilities()`.
- `platform.destroy` is cleanup, not gameplay or SDK telemetry.
- `game.ready` remains a wrapper method for portal SDK calls such as Playgama
  `game_ready`, but it is not a required analytics event because
  `game.loading_finished` already marks the first interactive handoff.
- `game.loading_progress` is not an event. Loading progress is a platform/web
  shell lifecycle signal from `platform_sdk_game_loading_progress()`, not
  telemetry for analytics subscribers.
- `session.end` belongs to the app/game lifecycle, not platform-sdk.
- `save.write` belongs to game-state/save code; platform-sdk may emit only
  platform storage operation failures as warnings or health counters if needed.
- `storage.load.request/result` and `storage.save.request/result` are not
  required events. Save/load can happen often enough to spam the event stream;
  game-state owns save/write analytics, and platform storage should stay an API
  detail unless a later health feature needs aggregate counters.
- `track`/`measure` forwarding is not an event. Analytics reads game events; an
  adapter may translate selected events to portal analytics internally from the
  `features/game-events` bridge. The platform wrapper must not expose a second
  event bus through `track()`.
- `input.first` is an input/game event. Platform-sdk may consume it as a
  precondition for `platform_sdk_gameplay_start()`, but it is not SDK-originated
  telemetry.
- `gameplay.start.blocked` should be a dev warning, not an analytics event. The
  facade should refuse the invalid call and warn in debug/dev builds.
- `sdk.pause`, `sdk.resume`, `sdk.audio_toggle`, and `sdk.visibility_change` are
  runtime callbacks, not required events. The game reacts by pausing/resuming
  simulation, input, and audio as appropriate; if that changes game state, the
  game layer emits the game-owned lifecycle events.
- `game.loading_finished` is not an asset streaming event. If a later feature,
  level, skin, locale, or downloadable content pack loads after the first
  interactive screen, emit asset/loader events from the asset system instead.
  If that load blocks active play, surround the blocking interval with
  `platform_sdk_gameplay_stop()` and `platform_sdk_gameplay_start()` as
  appropriate, but do not call `platform_sdk_game_loading_finished()` again.

Scorecards may consume a mixed stream containing these platform-sdk events plus
game/app events such as `first_60s.complete`, `gameplay.stop`, `items.txn`,
`items.move`, `progression.levelup`, `progression.xp_added`,
`progression.level_set`, and `progression.reset`. Session length is derived from
timestamps in the exported NDJSON stream, not from a required `session.end`
event.

## Platform SDK Adapters

### `mock`

Used by `local` and `itch`.

- Provides deterministic local SDK behavior for development and automation.
- In `local`, may simulate successful/failed ads when tests opt in.
- In `local`, interstitial and rewarded calls display an in-game mock overlay
  through the C/UI layer. Rewarded grants only on explicit success; skip, close,
  decline, and fail never grant.
- In `itch`, production behavior is no-op/unsupported for ads because itch.io
  has no required game SDK.
- Does not own analytics emission; the C facade will bridge SDK-originated
  lifecycle/ad-flow events into `features/game-events`.
- Must not require network access.

### `poki`

- Loads the Poki SDK script from the official CDN and calls `PokiSDK.init()`.
- Forwards monotonic loading progress to `PokiSDK.gameLoadingProgress()` when
  that SDK method exists.
- Calls `gameLoadingFinished()` when the game can be played.
- Uses `gameplayStart()` and `gameplayStop()` around active play, pause, menus,
  settings, dialogs, game over screens, and ads. `gameReady()` or first menu
  render is not a gameplay start.
- Uses `commercialBreak()` for natural interstitial opportunities.
- Uses `rewardedBreak()` only after clear user opt-in.
- Forwards selected finite typed game events through the official
  `PokiSDK.measure(category, what, action)` call. Other adapters safely no-op.
- Use the Poki Inspector for SDK event checks before submission.

### `yandex`

- For Yandex-hosted archives, load `/sdk.js`. For custom-domain integration,
  load the absolute Yandex SDK URL (`https://sdk.games.s3.yandex.net/sdk.js`).
- Initialize with `YaGames.init()`.
- Call `ysdk.features.LoadingAPI?.ready()` only when the game is ready for
  interaction.
- Use `ysdk.features.GameplayAPI?.start()` and `.stop()` to mark active
  gameplay.
- Forward `game_api_pause` and `game_api_resume` as runtime callbacks to the C
  facade. The C facade dispatches pause/resume listeners; game code decides how
  simulation, input, and audio respond.
- Use `ysdk.adv.showFullscreenAdv()` for interstitial ads and
  `ysdk.adv.showRewardedVideo()` for rewarded ads.
- If using Yandex player data, keep save payloads below documented limits and
  throttle writes.

### `playgama`

- Loads `https://bridge.playgama.com/v1/stable/playgama-bridge.js`.
- Initializes with `bridge.initialize()`.
- Calls `bridge.platform.sendMessage("game_ready")` after all loading screens
  are gone.
- Sends Playgama gameplay messages with the documented platform-message names:
  first gameplay start uses `level_started`, resume after a stop uses
  `level_resumed`, and gameplay stop uses `level_pause`.
- Forwards Playgama advertisement/runtime state changes as callbacks to the C
  facade. The C facade dispatches pause/resume listeners; game code decides how
  simulation, input, and audio respond.
- Uses `bridge.advertisement.showInterstitial()` for interstitials.
- Uses `bridge.advertisement.showRewarded()` and rewards only on the `rewarded`
  state.
- Uses `bridge.storage.get/set` only through the wrapper.

## First Implementation Slices

1. Add mock SDK fixture and wrapper facade.
2. Add target-to-SDK mapping tests: local->mock, itch->mock, poki->poki,
   yandex->yandex, playgama->playgama.
3. Add fake injected SDK tests for Poki, Yandex, and Playgama adapters.
4. Add publish target manifests for `itch`, `poki`, `yandex`, and `playgama`.
5. Add scorecard CLI over local NDJSON/export fixtures.

## Current Commands

```powershell
node --test features/platform-sdk/tests/platform_sdk.test.mjs
node features/platform-sdk/scripts/scorecard.mjs --input features/platform-sdk/tests/fixtures/scorecard-sample.ndjson --pretty
node features/platform-sdk/scripts/artifact_tools.mjs inspect --target poki --artifact templates/template/build/wasm-release-poki/bin
```

## Sources

- itch.io HTML5 games: https://itch.io/docs/creators/html5
- Poki SDK docs: https://sdk.poki.com/html5
- Yandex Games SDK connection: https://yandex.com/dev/games/doc/en/sdk/sdk-about
- Yandex gameplay/loading: https://yandex.com/dev/games/doc/en/sdk/sdk-game-events
- Yandex advertising: https://yandex.com/dev/games/doc/en/sdk/sdk-adv
- Yandex player data: https://yandex.com/dev/games/doc/en/sdk/sdk-player
- Playgama SDK setup: https://wiki.playgama.com/playgama/sdk/engines/core-plain-js/setup
- Playgama platform parameters: https://wiki.playgama.com/playgama/sdk/engines/core-plain-js/platform-parameters
- Playgama advertising: https://wiki.playgama.com/playgama/sdk/engines/core-plain-js/advertising

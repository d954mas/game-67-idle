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

The game should call only this wrapper. It must not call `window.PokiSDK`,
`YaGames`, `bridge`, or platform globals directly.

```ts
type TargetPlatform = "local" | "itch" | "poki" | "yandex" | "playgama";
type PlatformSdkId = "mock" | "poki" | "yandex" | "playgama";

const TargetPlatform = Object.freeze({
  LOCAL: "local",
  ITCH: "itch",
  POKI: "poki",
  YANDEX: "yandex",
  PLAYGAMA: "playgama",
});

const PlatformSdkId = Object.freeze({
  MOCK: "mock",
  POKI: "poki",
  YANDEX: "yandex",
  PLAYGAMA: "playgama",
});

type PlatformCapabilities = {
  target: TargetPlatform;
  platformSdk: PlatformSdkId;
  interstitial: boolean;
  rewarded: boolean;
  banner: boolean;
  cloudSave: boolean;
  platformAnalytics: boolean;
  locale: boolean;
  externalLinksAllowed: boolean;
};

type AdResult = {
  supported: boolean;
  shown: boolean;
  reason?: "unsupported" | "not_ready" | "rate_limited" | "failed" | "skipped";
};

type RewardedResult = AdResult & {
  rewarded: boolean;
};

type PlatformEvent = {
  name: string;
  phase?: "start" | "complete" | "fail" | "visible" | "interact";
  value?: string | number | boolean;
  data?: Record<string, unknown>;
};

interface PlatformSdk {
  ready(): Promise<boolean>;
  getTarget(): TargetPlatform;
  getPlatformSdk(): PlatformSdkId;
  capabilities(): PlatformCapabilities;

  gameReady(): Promise<void>;
  gameplayStart(data?: Record<string, unknown>): Promise<void>;
  gameplayStop(data?: Record<string, unknown>): Promise<void>;

  onPause(callback: () => void): () => void;
  onResume(callback: () => void): () => void;
  onVisibilityChange(callback: (hidden: boolean) => void): () => void;

  showInterstitial(placement?: string): Promise<AdResult>;
  showRewarded(placement?: string): Promise<RewardedResult>;
  showBanner?(placement?: string): Promise<AdResult>;
  hideBanner?(): Promise<void>;

  track(event: PlatformEvent): void;
  loadData(key: string): Promise<unknown | null>;
  saveData(key: string, value: unknown): Promise<void>;
  getLocale(): string | null;
  destroy(): void;
}
```

For C/WASM consumers, expose the same shape as C enums plus getters:

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
```

Use the runtime capability for gameplay/UI decisions. For example, a Telegram
link should be visible when `externalLinksAllowed` is true (`local`, `itch`),
and hidden when the target policy forbids external links (`poki`, `yandex`, and
any other restricted portal). Use `getTarget()` or `platform_sdk_target()` only
when the behavior is truly target-specific and cannot be expressed as a
capability.

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

Implementation rules:

- Keep platform target and platform SDK in one compile-time config module or
  build define.
- The wrapper facade may be common, but only the selected adapter is imported,
  compiled, or linked.
- Adapter modules must have no side effects at import time. SDK script loading
  happens only after the selected adapter is constructed.
- Mock test helpers are included only in `local`, `itch`, debug, or test builds.
- Release validation must grep or inspect the built artifact for forbidden SDK
  URLs. Example: an `itch` build must not contain Poki, Yandex, or Playgama SDK
  URLs.
- Runtime identity still exists, but it reports the build-selected target/SDK;
  it is not the mechanism that keeps unused adapters out of the bundle.

## Required Semantics

- `ready()` never throws. It resolves `false` when the selected SDK is absent,
  blocked, or unavailable.
- Unsupported operations return `{ supported: false, shown: false,
  reason: "unsupported" }` or a no-op promise.
- Ad methods fire `onPause` before the ad can cover gameplay and `onResume`
  exactly once when control returns, including failures.
- Rewarded ads grant rewards only when the SDK confirms reward completion:
  Poki returns a success boolean, Yandex uses `onRewarded`, Playgama uses the
  `rewarded` state.
- `track()` always records local scorecard events. Platform analytics is an
  extra sink when the active SDK supports it.
- `local` and `itch` targets must work without network access through the
  `mock` SDK adapter.
- Platform adapter code may dynamically load SDK scripts, but script loading is
  owned by adapters, not by template HTML injection.
- `capabilities().externalLinksAllowed` is the UI gate for creator/community
  links. Do not scatter portal-name checks through game UI code.

## Scorecard Events

The wrapper should emit a local NDJSON-compatible event stream. These names are
the minimum for first scorecards:

| Event | When |
| --- | --- |
| `platform.ready` | SDK ready or unavailable result is known. |
| `game.ready` | Player can interact; loading screens are gone. |
| `gameplay.start` | Player enters active gameplay. |
| `gameplay.stop` | Gameplay pauses, ends, menu opens, or an ad is planned. |
| `ad.interstitial.request` | Game asks for an interstitial opportunity. |
| `ad.interstitial.result` | Interstitial shown, skipped, blocked, or failed. |
| `ad.rewarded.request` | Player opts into a rewarded ad. |
| `ad.rewarded.result` | Rewarded ad result; includes `rewarded: true/false`. |
| `save.write` | Game progress write attempted. |
| `session.end` | Page close, game stop, or explicit session finish. |

## Platform SDK Adapters

### `mock`

Used by `local` and `itch`.

- Provides deterministic local SDK behavior for development and automation.
- In `local`, may simulate successful/failed ads when tests opt in.
- In `itch`, production behavior is no-op/unsupported for ads because itch.io
  has no required game SDK.
- Always emits local scorecard events.
- Must not require network access.

### `poki`

- Loads the Poki SDK script from the official CDN and calls `PokiSDK.init()`.
- Calls `gameLoadingFinished()` when the game can be played.
- Uses `gameplayStart()` and `gameplayStop()` around active play, pause, menus,
  and ads.
- Uses `commercialBreak()` for natural interstitial opportunities.
- Uses `rewardedBreak()` only after clear user opt-in.
- Use the Poki Inspector for SDK event checks before submission.

### `yandex`

- For Yandex-hosted archives, load `/sdk.js`. For custom-domain integration,
  load the absolute Yandex SDK URL.
- Initialize with `YaGames.init()`.
- Call `ysdk.features.LoadingAPI?.ready()` only when the game is ready for
  interaction.
- Use `ysdk.features.GameplayAPI?.start()` and `.stop()` to mark active
  gameplay.
- Subscribe to `game_api_pause` and `game_api_resume` to pause/resume gameplay
  and audio during startup ads, fullscreen ads, rewarded ads, tab switches, and
  other platform interruptions.
- Use `ysdk.adv.showFullscreenAdv()` for interstitial ads and
  `ysdk.adv.showRewardedVideo()` for rewarded ads.
- If using Yandex player data, keep save payloads below documented limits and
  throttle writes.

### `playgama`

- Loads `https://bridge.playgama.com/v1/stable/playgama-bridge.js`.
- Initializes with `bridge.initialize()`.
- Calls `bridge.platform.sendMessage("game_ready")` after all loading screens
  are gone.
- Uses Playgama advertisement state events to pause/resume gameplay and audio.
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

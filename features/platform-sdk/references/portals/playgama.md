---
type: Reference Lesson
title: Playgama Release Guidelines (research packet)
description: Playgama Bridge SDK contract, bridge config, submission requirements, moderation blockers, and an audit of our unproven playgama adapter against the live v2 documentation and the shipped bridge source.
tags: [game-knowledge, playgama, playgama-bridge, web-release, sdk, qa, audit]
status: research
sources_checked: 2026-09-07
---

# Playgama release guidelines — primary-source packet

All claims below come from Playgama's own documentation (`wiki.playgama.com`),
the `playgama/bridge` GitHub repository, and the two live bridge bundles served
from `bridge.playgama.com`, read 2026-09-07. Where a statement is my inference
rather than Playgama's text or code, it is marked **[inference]**.

Nobody has shipped a Playgama build from this repository yet, so section 6 is an
audit of unproven code, not a report of observed failures.

Source index:

- S1 `https://wiki.playgama.com/playgama/quick-start` — 5-step publishing checklist
- S2 `https://wiki.playgama.com/playgama/submitting-a-game` — the upload form, field by field
- S3 `https://wiki.playgama.com/playgama/game-requirements/game-self-check` — the moderation-blocker checklist
- S4 `https://wiki.playgama.com/playgama/game-requirements/technical-requirements`
- S5 `https://wiki.playgama.com/playgama/game-requirements/advertising-requirements`
- S6 `https://wiki.playgama.com/playgama/game-requirements/user-experience-requirements`
- S7 `https://wiki.playgama.com/playgama/game-requirements/content-requirements`
- S8 `https://wiki.playgama.com/playgama/game-requirements/other-requirements`
- S9 `https://wiki.playgama.com/playgama/game-requirements/platform-specific-requirements` — per-partner size/orientation limits
- S10 `https://wiki.playgama.com/playgama/faq` (`/submitting-a-game`, `/game-moderation`) — timelines, QA Tool, re-moderation
- S11 `https://wiki.playgama.com/playgama/bridge-sdk/getting-started` — v2 is current, v1 is "obsolete"
- S12 `https://wiki.playgama.com/playgama/bridge-sdk/setup` — loader URL, npm, init
- S13 `https://wiki.playgama.com/playgama/bridge-sdk/config` — `playgama-bridge-config.json`
- S14 `https://wiki.playgama.com/playgama/bridge-sdk/api` — the six mandatory "Required steps"
- S15 `https://wiki.playgama.com/playgama/bridge-sdk/api/platform` — language, sendMessage table, pause, audio
- S16 `https://wiki.playgama.com/playgama/bridge-sdk/api/storage`
- S17 `https://wiki.playgama.com/playgama/bridge-sdk/api/advertisement` + `/interstitial`, `/rewarded`, `/banner`, `/advanced-banners`, `/adblock`
- S18 `https://wiki.playgama.com/playgama/bridge-sdk/api/player`
- S19 `https://wiki.playgama.com/playgama/bridge-sdk/api/leaderboards`
- S20 `https://wiki.playgama.com/playgama/bridge-sdk/api/device`
- S21 `https://wiki.playgama.com/playgama/bridge-sdk/migration` — v1 → v2 breaking changes
- S22 `https://wiki.playgama.com/playgama/bridge-sdk/changelog`
- S23 `https://wiki.playgama.com/playgama/llms-full.txt` — the full documentation corpus (the form S1–S22 was actually read in)
- S24 `https://bridge.playgama.com/v2/stable/playgama-bridge.js` — live loader, `version` getter returns `2.1.0`, 111 KB
- S25 `https://bridge.playgama.com/v1/stable/playgama-bridge.js` — live legacy loader, `version` getter returns `1.32.0`, 213 KB
- S26 `https://github.com/playgama/bridge` — README and `package.json` (`@playgama/bridge` 2.1.0, LGPL-3.0-or-later)
- S27 `playgama/bridge:src/lib/bridge-config/types.ts` and `BridgeConfig.ts` — the authoritative config field list and load/parse failure behaviour
- S28 `playgama/bridge:src/modules/advertisement/types.ts` and `InterstitialController.ts` — ad config schema and show-gating
- S29 `playgama/bridge:src/constants/eventName.ts` — `EVENT_NAME` literal values
- S30 `playgama/bridge:src/PlaygamaBridge.ts` and `src/lib/loading-screen/LoadingScreen.ts` — init order, `setGameLoadingProgress`, the built-in loading screen
- S31 `https://playgama.github.io/bridge-config-editor/` — the official config editor Playgama tells you to use
- S32 `https://developer.playgama.com/` — the developer/partner console (upload, QA Tool, dashboard)

---

## 1. SDK integration contract

### 1.1 Loader URL and version

Current stable loader (S12):

```html
<html>
    <head>
        <script src="https://bridge.playgama.com/v2/stable/playgama-bridge.js"></script>
    </head>
    <body>...</body>
</html>
```

The script assigns the global `window.bridge`. The npm alternative is
`npm i @playgama/bridge`; the npm build "inlines every platform adapter into a
single file, so nothing is fetched at runtime" and also assigns `window.bridge`,
so code written against the global keeps working (S12).

Versions checked live 2026-09-07: `v2/stable` serves `2.1.0`, `v1/stable` still
serves `1.32.0` (S24, S25; `get version(){return "2.1.0"}` / `"1.32.0"` in the
respective bundles). `package.json` on `main` is `2.1.0` (S26). Every v2 doc page
carries the banner "You are reading the documentation for Bridge SDK **v2**. If
you need the obsolete v1 documentation, see Documentation (v1)" (S11–S21) — v1 is
documented as obsolete, and its docs live in a separate archived space.

The v2 CDN build downloads only the adapter of the platform the game actually
runs on; the docs recommend the CDN and name npm as the choice "when the build
has to be self-contained — offline builds, a strict CSP, or a bundler that
manages every dependency" (S12).

**[inference]** For partner platforms whose rules say "no external calls"
(YouTube Playables, TikTok, GameSnacks — S9), the runtime adapter fetch is
Playgama's own infrastructure and is not what those rules target; but the npm
bundled build removes the question entirely and is the safer choice for a build
we intend to distribute to those partners.

### 1.2 Initialization

```javascript
bridge.initialize()
    .then(() => {
        // initialization was successful, SDK can be used
    })
    .catch(error => {
        // error, something went wrong
    })
```

"Call the initialization method and wait for it to finish before using any
`bridge.*` API" (S12). `initialize()` accepts an options object
(`PlaygamaInitOptions`): `configFilePath` overrides the config path, and the
whole options object is passed to the config loader as *fallback* values, so an
inline configuration can stand in when the JSON file fails to load (S30, S27).

Init order inside `initialize()` (S30): load config → detect platform id → merge
`platforms.<id>` overrides → set up loading visuals → create the platform bridge
→ initialize modules. On failure the promise rejects with a `BridgeError`
carrying `ERROR_CODE.INITIALIZATION_FAILED`.

### 1.3 Platform detection

Detection is automatic and internal: "When your game runs on a supported
platform, Bridge automatically loads that platform's native scripts and routes
API calls to it. In unsupported environments, including local development,
Bridge uses a mock platform: calls return safe defaults (`false`, `reject`, etc.)
instead of throwing" (S11).

`bridge.platform.id` reports the detected platform. `forciblySetPlatformId` in the
config forces one, which is how a build is tested against a specific platform
(S27). Supported platform ids seen in the config docs: `crazy_games`, `discord`,
`dlightek`, `facebook`, `game_distribution`, `gamepush`, `gamesnacks`, `huawei`,
`jio_games`, `lagged`, `microsoft_store`, `msn`, `ok`, `playgama`, `poki`,
`portal`, `reddit`, `samsung`, `telegram`, `tiktok`, `vk`, `xiaomi`, `y8`,
`yandex`, `youtube` (S13, S15).

### 1.4 The six required steps

S14 lists these as **mandatory**; "Platforms verify them during moderation — a
game that skips any of them can be rejected or behave incorrectly for players":

1. Wait for Bridge initialization before calling any SDK API.
2. Localize using `platform.language`, read once after initialization.
3. Save and load progress through **Storage** — "never use `localStorage`
   directly".
4. Subscribe to the **pause** and **audio** state events. "Platforms fire these
   events when the player switches tabs, minimizes the browser, or an ad opens —
   a game that keeps playing sound in the background fails moderation."
5. Send `platform.sendMessage('game_ready')` when the first playable frame is
   ready — "platforms use this message to hide their loading screen and start
   analytics".
6. Show **interstitial** ads at natural pauses — "platforms require them to
   qualify for revenue share".

### 1.5 Game loading progress and the built-in loading screen

v1's `game.setLoadingProgress(percent)` became `bridge.setGameLoadingProgress(percent)`
in v2 (S21).

Bridge renders **its own** full-screen loading overlay with the Playgama logo
unless the config sets `disableLoadingLogo: true` (S30,
`#setupLoadingVisuals`). Behaviour of that overlay (S30, `LoadingScreen.setProgress`):

- The game drives it with `bridge.setGameLoadingProgress(percent)`; at 100 the
  overlay hides.
- If the game never calls it, a fallback fires 700 ms after `initialize()`
  settles: `setProgress(100, true)`. The fallback is suppressed once the game has
  reported any progress (`if (isFallback && this.#currentProgress !== null) return`).

**[inference]** A game that does not forward loading progress therefore has
Playgama's loading screen disappear ~700 ms after SDK init while the wasm/assets
are still loading — the player looks at a blank canvas. Forwarding progress, or
setting `disableLoadingLogo: true` and owning the loading screen ourselves, are
the two coherent options.

### 1.6 Gameplay lifecycle messages

`bridge.platform.sendMessage(<id>)`. The documented ids (S15) are exactly:

| Message | Parameters | Meaning |
| --- | --- | --- |
| `game_ready` | none | Loaded, all loading screens passed, player can interact |
| `in_game_loading_started` | none | In-game load began (e.g. a level) |
| `in_game_loading_stopped` | none | In-game load finished |
| `player_got_achievement` | none | Significant moment (boss, record) |
| `level_started` | optional `{"world","level"}` | Gameplay started |
| `level_completed` | optional `{"world","level"}` | Gameplay won |
| `level_failed` | optional `{"world","level"}` | Gameplay lost |
| `level_paused` | optional `{"world","level"}` | Settings menu or pause button |
| `level_resumed` | optional `{"world","level"}` | Returned from menu / unpause |

There is no `level_pause`. Grepping both live bundles for `level_` yields exactly
`level_completed`, `level_failed`, `level_paused`, `level_resumed`,
`level_started` in v1 and v2 alike (S24, S25).

Relay matrix (S15): `game_ready` reaches `dlightek`, `facebook`, `gamesnacks`,
`playgama`, `poki`, `portal`, `telegram`, `tiktok`, `xiaomi`, `yandex`,
`youtube`. The `level_*` messages reach `crazy_games`, `poki`, `yandex`
(plus `gamesnacks` for `level_completed`/`level_failed`). On any other platform
the call is a silent no-op.

Unknown ids are not an error: `sendCustomMessage` exists for game-specific ids
and "Bridge treats it like a built-in message" (S15). **[inference]** So sending
`level_pause` does not throw — it is silently delivered as a custom message and
never relayed as a pause to Poki/Yandex/CrazyGames.

`game_ready` also starts the interstitial `initialInterstitialDelay` clock: the
interstitial controller records `#initTime` when it observes
`PLATFORM_MESSAGE.GAME_READY` (S28).

### 1.7 Pause and audio state

```javascript
bridge.platform.isPaused
bridge.platform.isAudioEnabled

bridge.platform.on(bridge.EVENT_NAME.PAUSE_STATE_CHANGED, isPaused => { /* ... */ })
bridge.platform.on(bridge.EVENT_NAME.AUDIO_STATE_CHANGED, isEnabled => { /* ... */ })
```

"On game start, check the current value and mute the game audio if it is `false`.
Subscribing to the event alone is not enough — it only fires on subsequent
changes, so the initial state must be applied manually" (S15).

Bridge aggregates pause/audio across sources (`interstitial`, `rewarded`,
`visibility`, `platform`, `rate`) and raises one event, so a single universal
handler covers ad opens, tab switches and system pauses; the ad docs explicitly
tell you to use these instead of reacting to each ad state (S17, and the
`_pauseStateAggregator` construction visible in S24).

### 1.8 Advertisement

`EVENT_NAME` literal values, from the source of truth (S29):

```
INTERSTITIAL_STATE_CHANGED: 'interstitial_state_changed'
REWARDED_STATE_CHANGED:     'rewarded_state_changed'
BANNER_STATE_CHANGED:       'banner_state_changed'
AUDIO_STATE_CHANGED:        'audio_state_changed'
PAUSE_STATE_CHANGED:        'pause_state_changed'
ORIENTATION_STATE_CHANGED:  'orientation_state_changed'
```

Modules expose `on` / `off` / `once` / `emit` (S27 `EventBus.applyLocalEventMixin`).

**Interstitial** (S17):

```javascript
bridge.advertisement.isInterstitialSupported          // boolean
bridge.advertisement.interstitialState                // 'loading' | 'opened' | 'closed' | 'failed'
bridge.advertisement.minimumDelayBetweenInterstitial  // seconds, default 60
bridge.advertisement.setMinimumDelayBetweenInterstitial(30)
bridge.advertisement.showInterstitial(placement)      // placement optional
bridge.advertisement.preloadInterstitial(placement)
```

`show()` gating, read from the controller (S28): the state goes to `loading`
first, then to `failed` without ever opening when (a) interstitials are disabled
or unsupported, (b) `initialInterstitialDelay` has not elapsed since `game_ready`,
or (c) the `minimumDelayBetweenInterstitial` timer (started on the previous
`closed`) is still running. So `failed` is the ordinary throttle outcome, not
only a network error.

**Rewarded** (S17):

```javascript
bridge.advertisement.isRewardedSupported
bridge.advertisement.rewardedState        // 'loading' | 'opened' | 'closed' | 'rewarded' | 'failed'
bridge.advertisement.rewardedPlacement
bridge.advertisement.showRewarded(placement)   // placement optional
bridge.advertisement.preloadRewarded(placement)
```

"Reward the player **only** when the state is `rewarded`. Granting on `closed`
lets players claim rewards without watching the ad" (S17).

Ordering: the core treats `loading`, `opened`, `rewarded` as "ad in progress" and
`closed` / `failed` as terminal, and `_setRewardedState` keeps the pause/audio
aggregator asserted while the state is `opened` **or** `rewarded` (S24, S25). So
`rewarded` precedes `closed`; `closed` is the terminal event.
**[inference]** A per-platform adapter that emits `rewarded` last and never
`closed` would leave a "wait for closed" consumer hanging — a consumer should
latch the reward on `rewarded` and settle on `closed` *or* on a timeout, never
discard a latched reward.

**Banner** (S17):

```javascript
bridge.advertisement.isBannerSupported
bridge.advertisement.bannerState               // 'loading' | 'shown' | 'hidden' | 'failed'
bridge.advertisement.showBanner(position, placement)  // position 'top' | 'bottom', default 'bottom'
bridge.advertisement.hideBanner()
```

Advanced Banners (multiple placements, custom positions, config-driven show/hide
tied to platform messages) and `bridge.advertisement.checkAdBlock()` (a promise
resolving to a boolean, informational only) also exist (S17).

### 1.9 Storage

v2 removed the storage-type concept; "Bridge automatically picks the most
suitable place to store data on the current platform, including cloud saves when
they are available. No configuration is required from the game" (S16).

```javascript
bridge.storage.get(['key_1', 'key_2']).then(data => { /* data[n] === null when absent */ })
bridge.storage.set(['key_1', 'key_2'], ['value_1', 'value_2'])
bridge.storage.delete(key)
```

Signatures changed between majors (S21):

| v1 | v2 |
| --- | --- |
| `get(key, storageType, tryParseJson)` | `get(key, tryParseJson = true)` |
| `set(key, value, storageType)` | `set(key, value)` |
| `delete(key, storageType)` | `delete(key)` |
| `isSupported()`, `isAvailable()`, `defaultType`, `bridge.STORAGE_TYPE` | removed |

The migration page carries an explicit warning: "The 2nd argument of `get()` is
now `tryParseJson`, not a storage type. Calls like `get('key', someType)` will
silently reinterpret that value — audit them."

Cloud storage stays gated behind player authorization unless
`storage.allowAnonymousCloudSave` is set (S27). `PLATFORM_STORAGE_AVAILABILITY_CHANGED`
fires when that availability flips (S29).

### 1.10 Player / auth, language, device, leaderboards

Player (S18): `bridge.player.isAuthorizationSupported`, `.isAuthorized`,
`.isGuest`, `.id`, `.name`, `.photos`, and `bridge.player.authorize(options)`
returning a promise. `authorize()` must be called from a direct player action.

Language (S15): `bridge.platform.language`, ISO 639-1 (`ru`, `en`), falling back
to the browser language when the platform provides none. Supported on all 25
platforms.

External-boundary getters (S15, new in v2 per S21):
`bridge.platform.isExternalCallsSupported` (whether outgoing HTTP is allowed at
all) and `bridge.platform.isExternalLinksAllowed` (whether to show buttons that
lead off-platform).

Device (S20): `bridge.device.type`, `.os`, `.orientation`
(`'portrait' | 'landscape'`, Plain JS only), `ORIENTATION_STATE_CHANGED`,
`SCREEN_SIZE_CHANGED`, plus safe-area insets. Bridge can render the mandated
"rotate your device" overlay itself:

```json
{ "device": { "useBuiltInOrientationPopup": true, "supportedOrientations": ["landscape"] } }
```

Leaderboards (S19): `bridge.leaderboards.type` is one of `not_available`,
`in_game`, `native`, `native_popup`; `setScore(id, score)`,
`getEntries(id)` (only when type is `in_game`; entries carry
`id, name, photo, score, rank`), `showNativePopup(id)` (only when type is
`native_popup`). Ids come from the config, and `isMain: true` marks the single
primary board used on `jio_games` and `youtube`. Per S19 (read 2026-09-08)
`in_game` is served on `y8` and `yandex`, `native` on `gamesnacks`,
`jio_games`, `lagged`, `msn`, `youtube`, `native_popup` on `facebook`;
`playgama.com` itself answers `not_available`.

Adapter mapping (`web/adapters/playgama.js`): `leaderboardCaps` reads the
type on every call, after the bridge is up — `in_game` → read + write,
`native` → write, `native_popup` → write + popup, `not_available` → nothing,
and nothing is cached because the host platform, not the build, decides.
`submitScore` calls `setScore(id, score)`; `fetchEntries` calls
`getEntries(id)` and maps rows to `{ value: score, rank, name, avatarUrl:
photo, extra: "" }` with `you` matched against `bridge.player.id`;
`showLeaderboard` calls `showNativePopup(id)`. A rejected bridge promise is
`failed`; a type that does not serve the call is `unsupported`. **[docs
silent]** on whether writes need a signed-in player; `needsLogin` is answered
false.

### 1.11 Analytics

There is **no** game-facing analytics/measure hook in Bridge. The opposite is
true: embedded analytics is a rejection reason — "The game includes integrated
analytics such as Google Analytics (GA4)" (S3), "The game does not include
embedded analytics systems like Google Analytics (GA4) or similar" (S4), and
YouTube, TikTok and GameSnacks each require "no external calls, including
built-in analytics" (S9). Bridge sends its own internal telemetry, gated by the
config flag `sendAnalyticsEvents` (S27).

---

## 2. `playgama-bridge-config.json`, field by field

"`playgama-bridge-config.json` is the SDK configuration file. It stores platform
identifiers, ad placements, the product catalog, leaderboards, and other SDK
settings. One file is shared by every platform: top-level values apply
everywhere, and the `platforms` block overrides them per platform" (S13).

Location for a plain-JS build: **next to `index.html`**; Bridge fetches
`./playgama-bridge-config.json`, relative to the page (S13, S27
`#defaultConfigFilePath`). Playgama tells you to author it with the config editor
at S31.

Failure behaviour (S27): a fetch error logs `CONFIG_LOAD_FAILED` and a JSON
syntax error logs `CONFIG_PARSE_FAILED`; in both cases the loader falls back to
the values passed into `initialize(options)` and **initialization continues**. An
unknown key is simply carried in the record (`ConfigFileOptions extends AnyRecord`),
so a wrong-schema file does not fail loudly — it fails as missing ad placements,
missing leaderboard ids and default delays.

Top-level fields, from the typed source of truth (S27 `types.ts`):

| Field | Type | Purpose |
| --- | --- | --- |
| `platforms` | `Record<platformId, ConfigFileOptions>` | Per-platform ids and overrides, deep-merged over the top level |
| `debug` | boolean | Enables Bridge logging (the `?debug` URL param overrides it) |
| `forciblySetPlatformId` | string | Forces platform detection — for testing |
| `remoteConfigUrl` / `remoteConfigTimeout` / `remoteConfigTtl` | string / number | Remote override of the config |
| `sendAnalyticsEvents` | boolean | Bridge's own telemetry |
| `disableLoadingLogo` | boolean | Suppresses Bridge's loading overlay entirely |
| `showFullLoadingLogo`, `showLoadingText` | boolean | Loading-overlay variants |
| `loadingSound` | `{ url }` | Branded sound played once over the loading screen |
| `game` | `{ adaptToSafeArea?: boolean, ... }` | Safe-area CSS application |
| `advertisement` | object | See below |
| `leaderboards` | `[{ id, isMain?, "<platformId>": "<nativeId>" }]` | Leaderboard id map |
| `achievements` | `[{ id, ... }]` | Achievement id map |
| `payments` | `[{ id, ... }]` | Product catalog |
| `tasks`, `dailyRewards`, `notifications`, `crossPromo` | objects/arrays | Optional engagement modules |
| `device` | `{ useBuiltInOrientationPopup?, supportedOrientations? }` | Orientation handling |
| `storage` | `{ allowAnonymousCloudSave?: boolean }` | Cloud save for guests |
| `saas` | object | SaaS beta |
| `videoPreviews` | `[{ image, videoId }]` | YouTube loading-screen preview |
| `disableAutoNotifications` | boolean | Suppresses built-in re-engagement notifications |

`advertisement` (S28 `AdvertisementOptions`, matching the doc examples in S17):

```json
{
    "advertisement": {
        "minimumDelayBetweenInterstitial": 60,
        "initialInterstitialDelay": 0,
        "useBuiltInErrorPopup": false,
        "useAdvertisementErrorPopup": false,
        "builtInErrorPopupCooldown": 0,
        "interstitial": {
            "disable": false,
            "preloadOnStart": "level_completed",
            "placementFallback": "level_completed",
            "autoShow": [],
            "placements": [ { "id": "level_completed", "yandex": "native_placement_id" } ]
        },
        "rewarded": {
            "disable": false,
            "preloadOnStart": "extra_life",
            "placementFallback": "extra_life",
            "placements": [ { "id": "extra_life", "yandex": "native_placement_id" } ]
        },
        "banner": {
            "disable": false,
            "placementFallback": "main_menu",
            "placements": [ { "id": "main_menu", "yandex": "native_placement_id" } ]
        },
        "advancedBanners": { "disable": false, "placementFallback": "..." }
    }
}
```

A placement id that has no entry in `placements` is passed through to the
platform unchanged (S28 `getPlatformPlacement`), so the array is a native-id
*mapping*, not an allow-list. Some `platforms.<id>` entries are marked
**required** in the docs: `game_distribution.gameId`, `microsoft_store.gameId`
and `microsoft_store.playgamaAdsId` (S13). Playgama's FAQ says they create the
GameDistribution id and add it to the build themselves (S10).

A minimum viable submission config for our shape of game is therefore: the
`advertisement` block with interstitial and rewarded placements, `leaderboards`
with our board ids, `device.supportedOrientations`, and the loading-screen
decision (`disableLoadingLogo` or forwarded progress). `platforms` may stay empty
for a Playgama-first submission.

---

## 3. Hard submission requirements

### 3.1 Archive, entry file, size

- "Attach a ZIP archive with your game. The archive root must contain
  `index.html`. The total game size inside the archive must not exceed 300 MB"
  (S2).
- "An `index.html` file is added to the root of the archive. File and folder
  names use only Latin characters." (S4, item 10)
- "The total size of all game files does not exceed 300 MB when uploaded as an
  archive." (S4, item 9)
- Games are hosted on Playgama servers; iframe hosting on your own server
  requires a written request to `developer.success@playgama.com` (S10).

Tighter partner limits (S9), which matter because distribution is selected per
platform at submission (S2):

| Platform | Size / file limits | Other hard constraints |
| --- | --- | --- |
| Facebook | ≤200 MB, ≤500 files | Brotli/Gzip; Unity decompression fallback off; load <3 s |
| MSN | <50 MB | Landscape, or landscape + portrait |
| YouTube Playables | initial build ≤30 MB, ≤8000 files | Portrait **and** adapted landscape, no black side bars, no external calls, filenames restricted to letters, digits, `.`, `-`, `_` |
| TikTok | ≤30 MB | Portrait, iOS+Android, no external calls |
| Xiaomi | ≤100 MB | Loads under 15 s, Android |
| GameSnacks | ≤100 MiB total, initial <15 MiB, every file <10 MiB, save <3 MiB (should be <500 KiB) | 9:16 portrait mandatory, playable in <15 s on 10 Mbps, no external calls |
| Playhop / Yandex | — | Russian language required |

### 3.2 External requests and analytics

"Registration or authorization on external services is not required for
launching and using the game" and "The game does not include embedded analytics
systems like Google Analytics (GA4) or similar" (S4). GameDistribution
additionally forbids "any outgoing links, including redirects, Google Analytics,
other analytics, or social media links" (S9). The game must not restrict itself
by the URL it is opened from (S4, item 7). Video may only be embedded in a way
that cannot lead the player to an external resource — a YouTube player is
explicitly not acceptable (S7).

### 3.3 Ads

From S5, all mandatory:

- Ads are displayed only through the Playgama Bridge; no third-party ads,
  including static ad images and text; no custom RTB banners. Additional ad
  blocks may only be sticky banners.
- Payments only through Playgama Bridge; no external purchases (S5, S7).
- Ads at logical pauses only; never under the player's finger or over an element
  the player wants to press.
- "When showing full-screen ads (interstitial or rewarded video), the game sound
  and gameplay must be paused."
- Progress survives an ad round trip.
- Ad blocks match the game's orientation.
- Rewarded is opt-in via a button whose text makes clear that an ad is coming and
  what the reward is; the reward must be an *additional* bonus, not a
  continuation gate; "It is forbidden to offer reward-based ads to 'gain +1 life'
  every time users lose a life."
- Troubleshooting names a fourth rewarded failure mode: "Missing ad attribution
  on the CTA button (missing 'AD' label or icon)" (S23).

### 3.4 Orientation, mobile, desktop

Mobile (S4): full-screen during gameplay or startup; visual elements must not
deform on rotation or resize; progress is preserved across an orientation
change; gesture control; no system video player; no WebGL notification on open.
If the game supports one orientation only, that value must be selected in the
draft, and the platform shows a rotate-device placeholder in the other one.

Desktop (S4): the active field stretches to the edge of the available area
(sticky banners excepted), aspect ratio of the active field does not exceed 1:2,
no distortion on resize, keyboard/mouse control by default.

Both: no page scrollbar, no cut-off or overlapping elements, every popup has a
close control, no technical messages/errors/freezes across rotation, long press,
swipes, minimize, ad open, and history navigation. Browsers: Chrome, Firefox,
Opera, Safari, Edge; Android 5.0+, iOS 9.0+ (S4).

Recommended but not mandatory: "home" and "pause" buttons, and supporting both
orientations (S8).

### 3.5 Sound, saves, language, content

- The game cannot be silent, must have a mute button, and must stop sound when
  the page is minimized (S6).
- Progress or high score must be saved, and saved through Bridge Storage
  (S6, S3, S14).
- Language selection must be reachable without reading the current language —
  flags or endonyms, universal icons on the path to it (S6).
- Content bans (S7, S8): realistic violence toward children or animals; politics,
  political figures, wars; religion and religious figures; predictions about the
  player's life, health or death; other people's copyright (names, brand logos,
  music) and characters from other games or films (Brawl Stars, Sprunki, Squid
  Game are named); real-money value, gambling, lotteries, external purchases;
  impersonating an official product.
- AI policy (S3, S7): AI-generated assets are allowed, but "Platforms will reject
  low-quality titles constructed wholly with generative AI that bring nothing
  fresh to the player experience". Playgama's footnote: AI-graphics titles
  perform "more than 2x worse on metrics"; story, logic and math via AI are fine.
- Load-time expectations are set per partner, not by Playgama itself: Facebook
  "preferably under 3 seconds", CrazyGames "no more than 5 seconds", Xiaomi
  "under 15 seconds", GameSnacks "playable in less than 15 seconds on any
  connection of at least 10 Mbps" (S9).

---

## 4. Store / listing assets and text

The upload form fields (S2), asterisk = required:

| Field | Notes |
| --- | --- |
| Title\* | "displayed in the catalog and used in search. Use a short, memorable 2-4 word name that matches the title shown in the game" |
| Game Engine\* | The engine used |
| Game Archive\* | ZIP, `index.html` at root, ≤300 MB |
| Description\* | Main idea, genre, what makes the game unique |
| How to play\* | Rules, controls, objectives |
| Supported Devices\* | Drives catalog filtering — unselected device families never see the game |
| Screen Orientation | Supported mobile orientation |
| Game Features | Feature checkboxes |
| Game Languages\* | "Specify all languages supported by your game" |
| Media | Screenshots, covers, icons, videos — **optional**, "good media improves catalog presentation" |
| Distribution\* | Playgama plus partner platforms; exclusions are selected here |
| Link | Existing publication, if any |

Support may adjust descriptions and other fields, "but accurate metadata helps
the game pass moderation faster" (S2).

**Playgama does not publish pixel sizes, formats, counts or character limits for
icon, cover, screenshot or video.** They are absent from the whole documentation
corpus (S23: no image-dimension spec appears anywhere), and the `platforms` doc
space has none either. The numbers are shown by the upload form itself in the
developer console (S32). Do not invent them; capture them from the form when the
draft is created, and record them back into this file.

Supported interface languages are likewise not enumerated. Two documented
constraints bind us instead: `platform.language` is ISO 639-1 (S15), and a
declared language that is not fully translated is a rejection reason — "The app
interface is not translated into all declared languages, or the interface
language does not match the game language" (S3). Declaring fewer languages is
safer than declaring aspirational ones.

---

## 5. Submission funnel and rejection reasons

Funnel (S1, S2, S10):

1. Register at `developer.playgama.com` and verify the email.
2. Integrate Bridge and pass the self-check (S3).
3. "Add Game", fill the form, attach the ZIP.
4. Click **Test Game** — the QA Tool opens and tests the game against platform
   requirements, "in different display modes and with various input parameters,
   such as language settings". "Save as Draft" parks it.
5. **Submit Game**. Status appears on the Dashboard. For a first game, the
   moderation team makes contact within a few days and opens a chat in your
   preferred messenger.
6. Playgama moderation takes **1–5 business days** (S2) / "usually 3-5 business
   days" (S10); partner portals up to **2 weeks**.
7. Payouts are reported and paid from the one Playgama account.

Updates re-enter moderation: "after making changes and uploading a new archive to
our server, the game must pass moderation again... The new version of the game
will be available to users only after successful moderation. Until then, the
previously uploaded version will be available in the catalog" (S10).

Rejection reasons, verbatim from the self-check (S3):

- Bridge is not integrated, the Game Ready event is not sent, or progress is not
  saved through SDK methods.
- The game is a low-quality title built entirely with generative AI.
- The interface is not translated into all declared languages, or the interface
  language does not match the game language.
- Ads configured incorrectly: they disrupt gameplay, fail to pause sound, or do
  not grant the promised reward.
- Sound does not pause when an ad plays or the screen is minimized.
- Progress is not properly saved.
- Technical messages or errors, crashes, freezes.
- The app name does not match between the game and the draft materials in one
  language.
- Bugs or issues that hinder usability.
- Important interface elements cut off by the game-area boundaries.
- Copyright violations (names, brand logos, music).
- The game fully or partially copies another game in the catalog.
- Integrated analytics such as GA4.

Note that four of the thirteen are SDK-integration defects and two more are the
pause/audio contract — the same six required steps from §1.4.

---

## 6. Audit: our adapter and config vs. the live documentation

Files audited:

- `C:\projects\game-67-idle\features\platform-sdk\web\adapters\playgama.js`
- `C:\projects\game-67-idle\features\platform-sdk\web\portal\playgama-bridge-config.json`
- `C:\projects\game-67-idle\features\platform-sdk\publish-targets\playgama.json`
- `C:\projects\game-67-idle\features\platform-sdk\references\contract.md`

### D1 — blocker — wrong platform message: `level_pause`

`playgama.js:120` sends `bridge.platform.sendMessage("level_pause")`. No such id
exists; the documented id is `level_paused` (S15), and neither live bundle
contains the string `level_pause` (S24, S25). Bridge accepts it as a *custom*
message, so it fails silently and is never relayed to Poki, Yandex or
CrazyGames. `contract.md:351` states the same wrong name and is the origin of the
defect.

### D2 — blocker — none of the required pause/audio wiring exists

`playgama.js:79-95` (`initBridge`) subscribes to nothing. The adapter never reads
`bridge.platform.isAudioEnabled` at start and never registers
`PAUSE_STATE_CHANGED` / `AUDIO_STATE_CHANGED` handlers. That is required step 4
(S14), a named rejection reason (S3: "Sound in the game does not pause when an ad
is played or when the screen is minimized"), and a mandatory ad rule (S5, item 8).
`contract.md:352-354` already promises this forwarding to the C facade; the
adapter does not implement it.

### D3 — major — legacy loader URL, and an adapter written against a mixture of v1 and v2

`playgama.js:1` and `publish-targets/playgama.json:46` pin
`https://bridge.playgama.com/v1/stable/playgama-bridge.js`, and `contract.md:345`
repeats it. v1 is documented as obsolete (S11–S21); stable v2 is
`https://bridge.playgama.com/v2/stable/playgama-bridge.js` and serves 2.1.0
(S12, S24). The v1 URL still resolves, so this fails as silent staleness rather
than a load error.

### D4 — major — storage call uses the v1 three-argument signature

`playgama.js:173`: `bridge.storage.get(key, undefined, false)`. In v2 the second
argument is `tryParseJson`, not a storage type, and there is no third argument
(S21). The migration page warns about exactly this call shape. Under v2,
`undefined` falls back to `tryParseJson = true`, so the SDK returns an already
parsed value and `playgama.js:176` then calls `JSON.parse` on an object, throws,
and returns the object through the catch — it happens to survive, by accident.
`playgama.js:184` (`storage.set`) is correct for both majors.

### D5 — major — loading progress is dropped, so Playgama's loading screen hides early

`playgama.js:198`: `gameLoadingProgress() {}`. Bridge v2 exposes
`bridge.setGameLoadingProgress(percent)` (S21) and drives its own logo overlay
from it; with no call, the fallback `setProgress(100, true)` fires 700 ms after
init and removes the overlay while our wasm and ntpacks are still loading (S30).
Either forward progress or set `disableLoadingLogo: true` in the config — the
current state does neither. `playgama.js:199` (`gameLoadingFinished`) is a
legitimate no-op given `gameReady()` sends `game_ready`.

### D6 — major — the submitted config is a placeholder with an invented schema

`playgama-bridge-config.json:1-5` contains `schema`,
`replace_before_submission` and `note` — none are Bridge fields (S27). Bridge
will fetch and parse it successfully and then find no `advertisement` block, so:
`minimumDelayBetweenInterstitial` silently defaults to 60 s (S17, S28), no
placement mapping exists for any platform, `disableLoadingLogo` is unset (see
D5), `device.supportedOrientations` is unset so the built-in rotate overlay never
appears (S20), and `leaderboards` is empty so any `setScore` uses an unmapped id
(S19). `publish-targets/playgama.json:13,21` requires the file to be present but
cannot check that it is real. The file must be authored through the config editor
(S31) before any submission.

### D7 — moderate — rewarded settle logic can strand a granted reward

`playgama.js:150-161`. The reward is latched on `rewarded` and the promise
settles on `closed`, which matches the core's own terminal-state model (S24,
S25) and the "reward only on `rewarded`" rule (S17) — correct in the normal case.
Two gaps: if a platform adapter's last emitted state is `rewarded` with no
`closed`, the promise waits the full `AD_TIMEOUT_MS` of 120 s
(`playgama.js:2`) and then resolves as `failed` with the reward discarded,
because the timeout path (`playgama.js:24`) ignores the latched flag. And the
`failed`-after-`rewarded` branch (`playgama.js:158-160`) returns
`shown: false, rewarded: true`, which the contract's event table
(`contract.md:243`) does not describe. Both are cheap to fix: have the cancel
path resolve with the latched reward.

### D8 — moderate — interstitial `failed` is reported as an error, but is the normal throttle result

`playgama.js:133` maps `failed` to `{ shown: false, reason: "failed" }`. Per
S28, `failed` is what the controller emits when the 60-second minimum delay has
not elapsed, when `initialInterstitialDelay` since `game_ready` has not elapsed,
or when interstitials are disabled in config — ordinary throttling, not a
failure. Our analytics and any retry logic will read it as breakage.

### D9 — moderate — banner is hardcoded unsupported

`playgama.js:204` (`hideBanner`) and `playgama.js:209-211` (`showBanner`) are
stubs returning `unsupported`. Bridge supports banners on Playgama
(`isBannerSupported`, `showBanner(position, placement)`, `hideBanner()`,
`BANNER_STATE_CHANGED` — S17). Banner is "highly recommended" rather than
required (S14), so this is a monetization gap, not a moderation blocker — but the
stub is a lie about the platform rather than about our game, and S5 permits
sticky banners as the one allowed additional ad block.

### D10 — minor — unsupported ads report `reason: "not_ready"`

`playgama.js:126` and `playgama.js:145` return `reason: "not_ready"` when
`isInterstitialSupported` / `isRewardedSupported` is false. `contract.md:199-200`
specifies `reason: "unsupported"` for unsupported operations. A permanently
unsupported format and a not-yet-initialized SDK are currently indistinguishable
to the caller.

### D11 — minor — capabilities Bridge exposes and we never surface

`platform_sdk_external_links_allowed()` (`contract.md:52`, `contract.md:97-100`)
has an exact Bridge counterpart, `bridge.platform.isExternalLinksAllowed`, plus
`isExternalCallsSupported` (S15, new in v2 per S21); the adapter exposes neither,
so a Playgama build has to fall back to a hardcoded target policy. Leaderboards
(S19) are also absent from the adapter although this studio's games ship them, and
`bridge.device.orientation` / `supportedOrientations` (S20) is the documented
route to the rotate-device requirement in S4.

### D12 — minor — publish target metadata is thin against the real rules

`publish-targets/playgama.json:36` (`zip_layout`) describes our file list but
does not encode the two rules moderation actually checks: `index.html` at the
archive **root** and Latin-only file and folder names (S4, item 10), nor the
300 MB ceiling (S2). `metadata.orientation: "game-owned"` (line 41) is
inconsistent with S4, which requires the supported orientation to be declared in
the draft and handled with a rotate placeholder.

### What is already correct

`bridge.initialize()` with await (`playgama.js:86`) matches S12.
`bridge.platform.sendMessage("game_ready")` (`playgama.js:105`) is required step
5 and is the message the ad controller uses to start its initial-delay clock
(S14, S28). The `EVENT_NAME` lookup with literal fallbacks
(`playgama.js:75-77, 130, 150`) resolves to exactly the values in S29.
`bridge.advertisement.on/off` exist (S27 `EventBus`). First gameplay start as
`level_started` and resume as `level_resumed` (`playgama.js:112`) match S15.
`measure() {}` as a no-op (`playgama.js:206`) is right for a platform where
third-party analytics is a rejection reason (S3, S4).

### Fix order

D1, D2 first — they are the two defects that fail moderation on their own. Then
D3+D4+D5 as one v2 migration. Then D6, which cannot be finished without the
console (placement ids and the orientation decision). D7–D12 after that.

---

## Not covered by Playgama's docs (do not invent a requirement)

- Pixel sizes, formats, file-size caps, counts and character limits for icon,
  cover, screenshots and video. Absent from the whole corpus; read them off the
  upload form in the console and record them here.
- The list of catalog-supported interface languages.
- Whether the QA Tool blocks submission on a failed check, or only reports.
- Any documented revenue share, minimum-performance or contractual KPI. Playgama
  says only that platforms "require [interstitials] to qualify for revenue share"
  (S14) and that reports and payouts run through one account (S1).
- Whether a `rewarded` state is always followed by `closed` on every platform
  adapter. The core's terminal-state handling implies it (S24, S25), but the
  per-platform adapters are fetched at runtime and were not read.
- Playgama-specific load-time or frame-rate thresholds. Only partner platforms
  publish numbers (S9).

---
type: Reference Lesson
title: Yandex Games Release Guidelines (research packet)
description: Yandex Games publishing requirements with the portal's current numbering, the SDK lifecycle contract, the draft form's documented limits, moderation blockers, and a mapping onto this repo's platform-sdk wrapper contract.
tags: [game-knowledge, yandex, web-release, sdk, qa, moderation]
status: research
sources_checked: 2026-09-07
---

# Yandex Games release guidelines — primary-source packet

All claims below come from Yandex's own developer documentation
(`yandex.com/dev/games/doc/en/...`), read 2026-09-07. Where a statement is my
inference rather than Yandex's text, it is marked **[inference]**. Where the
documentation is silent, this packet says **[docs silent]** instead of guessing.
Where the only authority is the Developer Console — a form field, a validation
message, or observed console behaviour — it is marked **[console]**, and a
**[console]** claim that no documentation page confirms is flagged as such so it
can be re-checked against the live form rather than trusted.

Requirement numbers below are the portal's own, read off the live list on
2026-09-07 (S1). Yandex renumbers this list: several items are published as
"(Deprecated)" placeholders that keep their number, and other items have moved.
Never quote a number from memory or from an older studio document; read it off
S1 on the day.

Source index:

- S1 `https://yandex.com/dev/games/doc/en/concepts/requirements` — the requirement list, sections 1-8
- S2 `https://yandex.com/dev/games/doc/en/requirements/1/3.md` — requirement 1.3, sound outside the game, with the moderator's own test
- S3 `https://yandex.com/dev/games/doc/en/requirements/1/12.md` — requirement 1.12, YAN monetization, and how it is passed
- S4 `https://yandex.com/dev/games/doc/en/requirements/1/19.md` — requirement 1.19.x, SDK methods, and the debug-panel indicators moderation reads
- S5 `https://yandex.com/dev/games/doc/en/requirements/2/14.md` — requirement 2.14, automatic language detection
- S6 `https://yandex.com/dev/games/doc/en/sdk/sdk-about` — Connection and usage: the loader tag and `YaGames.init()`
- S7 `https://yandex.com/dev/games/doc/en/sdk/sdk-game-events` — Game Ready (`LoadingAPI.ready`) and gameplay markup (`GameplayAPI.start/stop`)
- S8 `https://yandex.com/dev/games/doc/en/sdk/sdk-events` — `game_api_pause` / `game_api_resume` and the other SDK events
- S9 `https://yandex.com/dev/games/doc/en/sdk/sdk-adv` — `showFullscreenAdv`, `showRewardedVideo`, sticky banner
- S10 `https://yandex.com/dev/games/doc/en/sdk/sdk-player` — `getPlayer`, `setData`/`getData`, `setStats`/`getStats`, and their quotas
- S11 `https://yandex.com/dev/games/doc/en/sdk/sdk-environment` — `ysdk.environment`, `i18n.lang`
- S12 `https://yandex.com/dev/games/doc/en/concepts/local-launch` — `@yandex-games/sdk-dev-proxy`, dev mode, mocks
- S13 `https://yandex.com/dev/games/doc/en/console/debug-panel` — the debug panel and every indicator it shows
- S14 `https://yandex.com/dev/games/doc/en/console/add-new-game/draft` — the draft form, field by field, with its published limits
- S15 `https://yandex.com/dev/games/doc/en/concepts/moderation` — moderation stages, timings, rejection, the doubling cooldown
- S16 `https://yandex.com/dev/games/doc/en/console/adv-monetization` — enabling ad monetization and YAN registration
- S17 `https://yandex.com/dev/games/doc/en/payments.md` — the unified licensing model (contract)
- S18 `https://yandex.com/dev/games/doc/en/concepts/languages-and-domains` — the 42 supported languages and the domain list
- S19 `https://yandex.com/dev/games/doc/en/console/test-game` — testing a draft before moderation
- S20 `https://yandex.com/dev/games/doc/en/console/add-new-game` — the upload flow
- S21 `https://yandex.com/dev/games/doc/en/llms.txt` — the full documentation index (used to resolve page URLs; the pause/resume events live on S8, not on S7)

Repo files this packet maps onto: `features/platform-sdk/references/contract.md`,
`features/platform-sdk/web/adapters/yandex.js`,
`features/platform-sdk/publish-targets/yandex.json`,
`features/platform-sdk/scripts/yandex_sdk_probe.mjs`,
`ai_studio/store_kit/yandex_spec.json`.

---

## 1. SDK integration contract

### 1.1 Loader and initialization

The documented shape is a static tag, and for an archive hosted by Yandex the
path is relative (S6):

```html
<!-- Yandex Games SDK -->
<script src="/sdk.js" async onload="initSDK()"></script>
```

S6 also documents a dynamic, non-blocking load. For a custom-domain integration
the absolute SDK path is used instead of the relative one (S6). The hard rule is
one sentence: "The `/sdk.js` script must be connected before executing
`YaGames.init()`" (S6).

```javascript
const ysdk = await YaGames.init();
// server-side signature verification of purchases:
const ysdk = await YaGames.init({ signed: true });
```

Requirement **1.19.1**: "The SDK is initialized exactly as specified on the
Connection and usage page. For the requirement to be considered fulfilled, the
current loader must be used" (S4). Moderation does not read the code — it reads
the debug panel's loader indicator: `W` while waiting, `IT` when the current
loader initialized correctly, `IF` when an outdated loader is in use (S4, S13).

**[inference]** An adapter that injects the `<script>` tag at runtime rather than
shipping it in `index.html` still produces `IT` if it fetches the same current
`/sdk.js` and initializes before use, because the indicator reports the loader
identity and its init, not the tag's position in the document. That is an
inference about the check, not a documented allowance; the settling evidence is
a draft upload and the panel's own verdict.

### 1.2 Lifecycle: the current numbers

| # | Requirement (verbatim, S1) |
| --- | --- |
| 1.19.1 | "The SDK is initialized exactly as described on the Connection and usage page." |
| 1.19.2 | "At the moment when the user can start playing the game, the `LoadingAPI.ready()` method from Game Ready must be called." |
| 1.19.3 | "If the game has gameplay markup, the moments when the `GameplayAPI.start()` and `GameplayAPI.stop()` methods are called correspond to those described in section Gameplay." |
| 1.19.4 | "If the game uses the `ysdk.on()` and `ysdk.off()` methods to track the `game_api_pause` and `game_api_resume` events, the event handling logic must correspond to the description in section Pause and Resume Events." |

Two numbers that older studio documents attach to these rules are wrong today:
**1.20** is not the lifecycle requirement — `1.20.1`-`1.20.4` is the
browser / OS / mobile OS / Android TV matrix (S1). The lifecycle lives entirely
under 1.19.x.

**Game Ready** (S7): "The method should be called when the game has loaded all
resources and is ready to interact with the user" — all elements interactive,
no loading screens still on top.

```javascript
const ysdk = await YaGames.init();
ysdk.features.LoadingAPI?.ready()
```

Moderation's test for 1.19.2 (S4): the panel's Game Ready indicator must go from
purple to green **within 90 seconds**, and the moment must match real
playability — not fired early while still loading, and not significantly after
the game became interactive (S4, S13).

**Gameplay markup** (S7). `GameplayAPI.start()` on level start, closing a menu,
unpausing, returning from an ad, and returning to the browser tab;
`GameplayAPI.stop()` on level completion or loss, opening a menu, pausing, a
fullscreen or rewarded ad, and switching tabs. "After sending the
`GameplayAPI.stop()` event, the gameplay is stopped" — resuming requires an
explicit `start()` (S7). Moderation reads the gamepad indicator: green during
play, red in menus, during ads, and on focus loss (S4).

### 1.3 Pause and resume events

The pause events are **not** on the gameplay-markup page; they are on the Events
page (S8, resolved through S21). Subscription is through the SDK object:

```javascript
ysdk.on('game_api_pause', pauseCallback);
ysdk.off('game_api_pause', pauseCallback);
ysdk.on('game_api_resume', resumeCallback);
ysdk.off('game_api_resume', resumeCallback);
```

The platform raises them for: showing and closing a fullscreen or rewarded ad;
opening and closing a purchase window; switching browser tabs; minimizing and
restoring the browser window (S8). On `game_api_pause` the game must mute audio
and pause gameplay, and wait for `game_api_resume` before resuming (S8).

Two behaviours that change how an adapter should be written:

- The platform itself calls `GameplayAPI.stop()` when `game_api_pause` fires and
  `GameplayAPI.start()` when `game_api_resume` fires (S8). Our own stop/start
  around the same window is therefore redundant but harmless.
- "If the game has already been stopped using the `GameplayAPI.stop()` method and
  then the `game_api_pause` event occurs, the `GameplayAPI.start()` method will
  not be called upon the subsequent `game_api_resume` event" (S8). The platform
  will not restart gameplay that the game had already stopped — which is exactly
  the rule our facade implements when it restores gameplay on resume only if it
  was active when the pause arrived.

**[docs silent]** on whether these events are also dispatched on `window`. S8
documents only `ysdk.on()` / `ysdk.off()`, and requirement 1.19.4 is written
against those two methods (S1). Our adapter binds
`window.addEventListener("game_api_pause", ...)`
(`features/platform-sdk/web/adapters/yandex.js:70-71`), which is an undocumented
channel; **[inference]** the documented subscription should be added alongside
it, because 1.19.4 and the moderator's gamepad check are phrased around the SDK
methods and an undocumented window event can stop being dispatched without
notice.

The other SDK events (S8): an `ESdkEventName` enum carrying `EXIT`,
`HISTORY_BACK` (TV only), and the account-selection dialog events
(`ACCOUNT_SELECTION_DIALOG_OPENED` / `..._CLOSED`, which matter when a player
switches between an authorized and an anonymous account).

### 1.4 Advertising API (S9)

```typescript
function showFullscreenAdv(callbacks?: {
    onOpen?: () => void;
    onClose?: (wasShown: boolean) => void;
    onError?: (error: object) => void;
}) => void {}

function showRewardedVideo(callbacks?: {
    onOpen?: () => void;
    onRewarded?: () => void;
    onClose?: (wasShown: boolean) => void;
    onError?: (error: object) => void;
}) => void {}
```

`onClose` for a fullscreen ad is "Called when the ad closes, after an error, or
after an ad failed to open due to **too frequent calls**" (S9) — so
`wasShown === false` is an ordinary throttle outcome, not a fault. "The frequency
of calling an interstitial ad unit is controlled by Yandex Games" (S9): the game
asks at logical pauses and the platform decides. **[docs silent]** on the numeric
interval; S9 states no minimum-seconds figure, and none should be written into
game code.

`onRewarded` is the only reward signal: "Called when a video ad impression is
counted. This function should specify a reward for viewing the ad" (S9).

S9's one placement rule, verbatim: "Do not call ads during gameplay, when users
may unintentionally click on the ad unit."

Sticky banner (S9):

```typescript
function getBannerAdvStatus(): Promise<{ stickyAdvIsShowing: boolean; reason?: 'ADV_IS_NOT_CONNECTED' | 'UNKNOWN'; }> {}
function showBannerAdv(): Promise<{ stickyAdvIsShowing: boolean; reason?: 'ADV_IS_NOT_CONNECTED' | 'UNKNOWN'; }>
function hideBannerAdv(): Promise<{ stickyAdvIsShowing: boolean; }>
```

`ADV_IS_NOT_CONNECTED` means "Banners are not enabled"; `UNKNOWN` is "Error
displaying ads on the Yandex side" (S9). The banner is drawn by the portal over
the game — there is no DOM container to own, and no size to choose.

### 1.5 Player data (S10)

```javascript
const player = await ysdk.getPlayer();            // or ysdk.getPlayer({ signed: true })
await player.setData(data, flush);
await player.getData(keys);
await player.setStats(stats);
await player.getStats(keys);
await player.incrementStats(increments);
player.isAuthorized();
```

Documented quotas (S10):

| Call | Limit |
| --- | --- |
| `getPlayer()` | 20 requests within 5 minutes |
| `setData` | maximum 200 KB per player; 100 times within 5 minutes |
| `getData` | 100 times within 5 minutes |
| `setStats` / `getStats` / `incrementStats` | maximum 10 KB of numeric data per player; 60 requests per minute |

`flush: true` sends immediately; `flush: false` (the default) queues (S10). Data
operations work for all players, authorized or not; `player.isAuthorized()`
reports which (S10).

### 1.6 Environment and language (S11)

```javascript
const ysdk = await YaGames.init();
const lang = ysdk.environment.i18n.lang;   // 'en', 'ru', ...
```

`ysdk.environment` carries `app.id`, `i18n.lang` (ISO 639-1), an optional
`payload` (the `payload=` query parameter of the game URL), and an optional
`referrer` object for launches from promo banners (S11). Of `i18n.lang` the docs
say: "Use this parameter to automatically detect the user's language in the
game" (S11).

### 1.7 What has no counterpart in our contract

- **No loading-progress API.** S7 documents only `LoadingAPI.ready()`; there is
  no percentage call. **[docs silent]**.
- **No `game_ready`-style second call.** `LoadingAPI.ready()` is the single
  handoff.
- **No game-facing analytics/measure call** in the SDK pages read (S6-S11).
  Portal-side analytics is a separate Yandex Metrica integration with its own
  documentation page (S21 index); it is not an SDK event sink comparable to
  `PokiSDK.measure()`.

Other SDK surfaces this packet does not map because our contract has no seam for
them, listed so nobody concludes they are absent: leaderboards, in-app
purchases, remote configuration, asynchronous multiplayer, game rating, desktop
shortcut, server time, links to other games (S21 index).

### 1.8 Local launch and the debug panel

Yandex ships its own proxy, so "cannot be tested locally" is never true (S12):

```
npm install -g @yandex-games/sdk-dev-proxy
npx @yandex-games/sdk-dev-proxy -p <path to the game folder>
npx @yandex-games/sdk-dev-proxy -p <path> --app-id=<game draft ID>
```

Flags (S12): `--host, -h` (a local server instead of a folder), `--path, -p`,
`--port` (default 8080), `--app-id, -i`, `--tld`, `--dev-mode` (default false).
Dev mode needs no registration and no draft; prod mode needs both, and for
security "supports only `localhost` domain in the `game_url` parameter". Dev mode
accepts mocks through the URL, e.g. `localhost:8080?mocks={...}` with
`canShowPrompt`, `isAuthorized`, `lockedOrientation` (S12).

The debug panel opens from the console ("Open with debug panel") or by appending
`&debug-mode=16` to the game URL, and appears bottom-left (S13). Its indicators
are the moderator's instrument, so they are ours (S13):

| Indicator | Meaning |
| --- | --- |
| Loader | `W` waiting, `IT` current loader initialized, `IF` outdated loader |
| Game Ready | purple blinking = SDK not initialized or `ready()` not yet called; green = called, with the elapsed ms; red = 90 s timeout |
| 文 (language) | automatic language detection through the SDK enabled or disabled |
| ▶️/⏸️ | simulates the platform's pause/resume events |
| 🎮 (gamepad) | emulates and reports `GameplayAPI.start()` / `stop()` state by colour |
| ⏱️ | controls the transparency of Yandex's own loading screen |

The panel also carries toggles for language, game-link mocking, focus, network
throttling, currency mocking and clearing cloud data (S13).

---

## 2. Mapping onto this repo's wrapper contract

The adapter object our facade expects is exactly: `destroy`,
`gameLoadingProgress`, `gameLoadingFinished`, `gameReady`, `gameplayStart`,
`gameplayStop`, `getLocale`, `hideBanner`, `loadData`, `measure`, `ready`,
`saveData`, `showBanner`, `showInterstitial`, `showRewarded`
(`features/platform-sdk/references/contract.md`,
`features/platform-sdk/web/adapters/yandex.js:234-252`).

| Adapter method | Yandex implementation | Source |
| --- | --- | --- |
| `ready()` | load `/sdk.js` (absolute SDK URL only for a custom domain), `await YaGames.init()`, and read `environment.i18n.lang` inside the same chain | S6, S11 |
| `gameLoadingProgress(p)` | no-op — no progress API exists | S7, **[docs silent]** |
| `gameLoadingFinished()` | `ysdk.features.LoadingAPI?.ready()`, once, when the player can actually start | S7, req. 1.19.2 |
| `gameReady()` | no-op — `LoadingAPI.ready()` is the only handoff | S7 |
| `gameplayStart()` | `ysdk.features.GameplayAPI?.start()` | S7, req. 1.19.3 |
| `gameplayStop()` | `ysdk.features.GameplayAPI?.stop()` | S7, req. 1.19.3 |
| pause / resume listeners | `ysdk.on('game_api_pause', cb)` and `ysdk.on('game_api_resume', cb)`, forwarded to the C facade, which stops gameplay, dispatches pause listeners and restores gameplay on resume only if it was active | S8, req. 1.19.4 |
| `showInterstitial(placement)` | `adv.showFullscreenAdv({onOpen,onClose,onError})`; `{shown:true}` on `onClose(true)`, `{shown:false, reason:"skipped"}` on `onClose(false)`, `{shown:false, reason:"failed"}` on `onError` | S9 |
| `showRewarded(placement)` | `adv.showRewardedVideo(...)`; latch the reward in `onRewarded` only, settle on `onClose` | S9, req. 4.5 |
| `showBanner()` | `adv.showBannerAdv()`; `shown = stickyAdvIsShowing`, else `reason` = `ADV_IS_NOT_CONNECTED` / `UNKNOWN` | S9 |
| `hideBanner()` | `adv.hideBannerAdv()` | S9 |
| `loadData(key)` | `player.getData([key])` | S10 |
| `saveData(key, value)` | `player.setData({[key]: value})`, ≤200 KB per player, ≤100 writes / 5 min | S10 |
| `getLocale()` | `ysdk.environment.i18n.lang`, read during init, cached | S11, req. 2.14 |
| `measure()` | no-op — no SDK event sink | S6-S11, **[docs silent]** |
| `destroy()` | cancel pending ad promises | repo |

`placement` from our contract has no Yandex counterpart: neither
`showFullscreenAdv` nor `showRewardedVideo` takes an argument (S9). Keep the
placement in our own event payload only.

Notes for the implementer:

- Ad callbacks are not promises and can silently never fire; keep the
  `adOperation(start, failedResult)` timeout wrapper so a portal that never calls
  back still settles.
- `onClose(false)` on a fullscreen ad is the documented outcome of "too frequent
  calls" (S9). Reporting it as an error skews our own analytics — it belongs in
  the same bucket as a throttle, not a failure.
- `saveData` must not be called per mutation. 100 writes per 5 minutes (S10) is
  one write every 3 seconds sustained; the debounce lives in game-state, not in
  the adapter.
- Binding window `blur` / `focus` to pause/resume is stricter than requirement
  1.3 asks for (see §3.6) and is safe, but it does not satisfy 1.19.4 — that
  requirement is about `ysdk.on()`/`ysdk.off()` (S1, S8).
- `publish-targets/yandex.json` should encode the two archive rules moderation
  actually checks (`index.html` at the archive root, no spaces or Cyrillic in
  file and folder names — req. 1.22) and the 100 MB uncompressed ceiling
  (req. 1.21, S14).

---

## 3. Hard technical requirements

Numbers are section 1 of S1 unless stated. Items published as "(Deprecated)"
keep their number and are not reused: 1.5, 1.17, 2.5, 2.11, 2.12, 3.1-3.3, 3.8,
5.5, 5.7, 5.8, 5.10, and the whole of section 7.

### 3.1 Archive layout, naming and size

- **1.22** "An index.html file has been added to the root of the archive. File
  and folder names do not contain spaces or Russian characters."
- **1.21** "The total size of all game files does not exceed 100 MB in
  uncompressed form."
- The draft form repeats both: "ZIP archive with the game", "uncompressed archive
  content weighs no more than 100 MB" (S14).

A `unzip -l` of the packaged archive answers both requirements on its own.

### 3.2 Absolute URLs

- **1.7** "In the interests of the game working correctly anywhere in the world,
  the program code doesn't use absolute URLs that link to the Yandex S3 servers."
- **1.18** "The game's operation isn't restricted due to the URL where it's
  open." Sitelocking a Yandex build is a rejection, not a defence.

The rule is narrow: it bans absolute URLs **to Yandex S3**, not every absolute
URL. A game's own backend endpoint is not what 1.7 targets. What does constrain
outbound behaviour is section 8.4 — links must not lead to external resources
(8.4.2) and redirecting users off-platform is prohibited (8.4.4); links
integrated via the SDK to the developer's other games in the catalogue are the
documented exception (8.4.1), together with 8.4.3's allowance for social-network
communities dedicated to the developer's catalogue games. **[inference]** A
data-only HTTPS request that never navigates the player anywhere is outside
8.4's subject matter, which is links and redirects; it is not an exemption
written anywhere, so a game that makes one should expect to be asked about it.

### 3.3 Context menu, selection, scrolling, zoom

- **1.6.1.8** (mobile) "A long tap in the game doesn't select an area or open the
  context menu."
- **1.6.2.7** (desktop) "Interacting with the internal game field does not lead
  to highlighting the field or opening the context menu."
- **1.10.2** "Browser page scrolling (using the system scroll bar) or
  swipe-to-refresh is not available. With that said, you can implement your own
  content scrolling in the game."
- **1.10.1** "Elements are not cut off and do not extend beyond the screen
  boundaries." **1.10.3** "Elements and texts don't overlap or cover other
  elements." **1.10.4** the main screen needs no scrolling or swiping and is
  one-hand playable.

"Context menus appearing on interaction" is on Yandex's own list of common
rejection reasons (S15).

**[docs silent]** on gesture zoom as a numbered requirement — the list has no
pinch-zoom item. It is covered indirectly by 1.10.x (a zoomed page cuts elements
off) and by the full-screen requirements 1.6.1.1 / 1.6.2.1. **[inference]** The
viewport meta tag remains the right fix; do not cite a requirement number for it.

**[inference]** The shell mechanics that satisfy these: a `contextmenu` handler
covers desktop and Android but not iOS, which needs
`-webkit-touch-callout: none` plus `user-select: none`; page scroll needs
`overflow: hidden` and `overscroll-behavior: none` on `html, body` plus
`touch-action: none` on the canvas.

### 3.4 Screen, orientation and platform matrix

- **1.6** the game must meet the requirements for the device types declared in
  the draft. **1.6.1.x** is mobile, **1.6.2.x** desktop, **1.6.3.x** TV.
- **1.6.1.1** full-screen during launch or gameplay; **1.6.2.1** the active area
  stretches to the available space, sticky banners excepted; **1.6.2.2** "The
  long side of the active area should not exceed the short side by more than two
  times."
- **1.6.1.3 / 1.6.2.3** nothing deforms or stretches disproportionally when the
  orientation or the available area changes.
- **1.6.2.4** playable with keyboard or mouse by default, independent of keyboard
  layout; **1.6.2.6** no OS-reserved shortcuts.
- **1.6.1.7** "There's no WebGL notification when the user opens the game."
- **1.6.1.6 / 1.6.2.5** the system video player must not appear in any browser —
  named again as a rejection reason in S15.
- **1.20.1-1.20.4** the support matrix: Yandex Browser, Chrome, Firefox, Opera,
  Safari and the Yandex mobile app; Windows Vista/7/8/10 and macOS 10.6+;
  Android 5.0+ and iOS 9.0+; Android TV.
- **1.6.3.x** applies only to a TV-declared draft: full screen, playable entirely
  with the remote's arrows, Back and OK handled, no in-game purchases, no links
  to the developer's other games.

### 3.5 Ads during gameplay

Section 4 of S1, verbatim numbering:

| # | Requirement |
| --- | --- |
| 4.1 | "Ads are only called via the Yandex Games SDK. There are no ads from third-party advertisers, including static ad images and text." |
| 4.2 | "If a user clicks an ad and returns to the game afterwards, their progress is saved." |
| 4.3 | "The orientation of all ad units matches the game orientation." |
| 4.4 | "Advertising does not interfere with interacting with the game and is shown only during logical pauses." |
| 4.5 | rewarded video is opt-in — the user watches "if they want to (for example, by pressing a special button)" |
| 4.5.1 | the button must be linked to text or another button that clearly indicates that an ad is about to be watched in exchange for a reward, and the specific reward |
| 4.5.2 | "The reward for watching an RV ad is an extra bonus ... that enhances the core game experience and doesn't affect the player's ability to continue playing." |
| 4.6.1 | "Additional ad units can only be sticky banners." |
| 4.6.2 | "Custom RTB banners are prohibited." |
| 4.7 | "During fullscreen ads (interstitial or rewarded videos), the game sound and gameplay must be paused." |

4.7 is two obligations, not one: muting is not enough, the simulation must stop
too. The platform's own `game_api_pause` fires for an ad (S8), so a game that
routes that event into a real gameplay pause satisfies 4.7 by construction.

**1.16** "The content and appearance of embedded ad units aren't changed. Service
ad units can't be imitated." **1.4** payments only through the SDK, no external
purchases of any kind.

### 3.6 Focus versus visibility for sound

**1.3** is one sentence — "When the game loses focus, the sound stops" — but the
moderator's test (S2) is narrower and more specific than the word "focus"
suggests. Sound must stop when:

- the browser window (desktop) or the app (mobile) is minimized;
- the player switches to another browser tab;
- the player opens the browser's tab-selection menu.

Documented exceptions (S2): audio may keep playing for up to **2 seconds** after
the player leaves the game tab; audio may persist after clicking an ad banner;
on iOS specifically it may keep playing in the tab-selection menu.

So the check the console runs is a **visibility** check, not a window-`blur`
check. **[inference]** Muting on `blur` as well is stricter than required and is
safe — the ad-banner exception means a blur-driven mute cannot fail the
requirement — but the requirement is not satisfied by a focus handler alone if
visibility changes do not reach the mixer, because a mobile browser backgrounding
the app is the case moderation actually tests. "Sound persisting when switching
tabs" and "audio continuing during ads" are both named rejection reasons (S15).

### 3.7 Saves

- **1.9** "In games with internal progress (new unlocked levels, records,
  achievements, upgrades), changes are saved right after the user performs an
  action or presses the save button. After refreshing the page or changing the
  screen orientation on a mobile device, the progress isn't lost."
- **1.11** "If the game uses cloud saves, the *The game use cloud save* option is
  enabled in the draft." The obligation runs one way: declaring cloud saves that
  the build does not implement is a false declaration, and a local-storage-only
  game leaves the box unchecked.
- **1.2.2** guest play must be possible and "The player's progress is saved in
  this case"; **1.2 / 1.2.1** no registration or third-party login is required,
  and any Yandex ID authorization happens only after a deliberate in-game action
  whose prompt states the benefit.
- **2.6** if the game has story/level progression it must be able to save
  progress; if it is endless or score-based, records must be saved.
- **1.13.3** — inside the in-app-purchase block only — "The game progress is
  saved on the server and available on different devices." **[inference]** A game
  with purchases therefore cannot stay local-storage-only; a game without them
  can.

### 3.8 Other section-1 items worth carrying

- **1.1** the SDK is installed in the game.
- **1.12** "Monetization is enabled: the game has ads or in-app purchases." S3
  softens this: moderation passes the game if purchases are enabled, **or** ad
  units (fullscreen, rewarded, or sticky banner — not counting the initial
  loading screen) are present, **or** the developer states a refusal to monetize
  in the console's *Developer's comment* field.
- **1.14** no technical errors, freezes or crashes; **6.4** no errors in the
  DevTools console (a recommendation, section 6, not a hard requirement).
- **1.15** finished look, not a development or preliminary-testing build.
- **1.23** "The use of interactive artificial intelligence (AI) in games is
  prohibited. The use of materials within the game that were pre-generated using
  AI is allowed."
- **1.24** "Game updates must preserve its core concept. It is prohibited to
  upload an entirely different game as an update."
- **2.9** a full playthrough takes more than 10 minutes, with the quiz example
  ("quizzes must have at least 100 questions") spelled out.
- **2.13** "The game Rating is above 30. If the game's rating remains at 30 or
  below (or is absent) for 3 weeks, it will be unpublished." This is a
  post-release requirement — publication is not the end of the requirement list.
- **3.6** not a partial or entire copy of another catalogue game, including the
  same developer's; **3.4** the content bans (magic and divination, realistic
  violence toward children or animals, politics, religion, predictions about a
  player's life or health); **3.5** the creator owns the rights to all materials;
  **3.7** no real money or non-game valuables, no store, gambling or lottery;
  **3.9** video may be embedded only in a way that cannot reach external
  resources — "Video integration via the YouTube player doesn't meet that
  condition."

---

## 4. Language: what the console actually checks

Two separate requirements, and games fail the second while passing the first.

**2.10** "The game is localized into at least one language selected in the draft.
The recommended minimum for publication is localization into Russian for RU, BE,
KK, UK, and UZ, and English for all other languages" (S1).

**2.14** "The game includes automatic language detection via SDK" (S1). The
dedicated page is explicit about scope and timing: detection must "occur during
launch, not during gameplay", and it must be "implemented in all games, even if
they declare only one language or have no text" (S5). The property is
`ysdk.environment.i18n.lang` (S5, S11).

How moderation verifies it (S5, S13): the moderator watches the debug panel's
language indicator go from red to green **at startup** — green means
auto-detection is enabled — then switches language through the panel's SDK mocks
dropdown, once per declared language, and checks that the translations load.

This is why a game running in flawless Russian is still refused. The check is not
"the game ended up in the right language". It is a **read of
`environment.i18n.lang` while the game loads**. A game that derives its language
from `navigator.language`, or asks the SDK once, late, or only on a run that has
no save, produces a red indicator and is rejected — "missing automatic language
detection" is on Yandex's own rejection list (S15).

The consequences for our adapter: read the locale inside the `YaGames.init()`
chain, cache it, and let the game adopt it on every launch until the player picks
a language by hand. `getLocale()` must return the portal's answer only — a
`navigator.language` fallback returned from the same function is
indistinguishable to the game from a portal answer, and hides the failure the
indicator is reporting.

Related: **6.9** (recommendation) manual language switching must work without
already knowing the current language — universal icons (gear, globe) and flags or
endonyms. **8.2.3** texts that vary by language and matter for gameplay must be
translated into that language. The catalogue supports 42 languages in ISO 639-1
(S18); the recommended set for traffic is Russian, Turkish, Chinese, Korean,
Hindi, Vietnamese and English (S18). Declaring a language means committing to it:
**5.1.3** requires the name to be identical across the game and all draft
materials **for each language selected in the draft**.

---

## 5. Store draft: fields, materials, and where the numbers come from

Unlike Playgama and CrazyGames, Yandex **publishes** most of its draft numbers.
S14 is the authority for everything in this table; the console form is still the
final word if it ever disagrees, and a disagreement should be recorded back into
`ai_studio/store_kit/yandex_spec.json` in the same commit.

| Field | Constraint (S14) | Required |
| --- | --- | --- |
| Title | maximum 50 characters, including punctuation and spaces | yes |
| Description | minimum 100, maximum 1000 characters | yes |
| How to play | minimum 100, maximum 1000 characters | yes |
| Version | default `0.0.0.1` | — |
| Categories | "No more than two categories" | yes |
| Icon | 512 × 512 px, PNG | yes |
| Cover | 800 × 470 px, PNG | yes |
| Screenshots | at least 2 per selected platform; 16:9 landscape, 9:16 portrait; long side 1280-2560 px; JPEG or 24-bit PNG | yes |
| Vertical video | 9:16, MP4, height from 400 px, up to 28 s, up to 100 MB | no |
| Horizontal video | 16:9, MP4, height from 400 px, up to 28 s, up to 100 MB | no |
| GIF, advertising videos | — | no |
| Archive | ZIP; uncompressed content ≤ 100 MB | yes |
| Supported platforms | Desktop / Mobile (iOS, Android) / TV | yes |
| Orientation | Portrait / Landscape / Any | yes |
| Game translated into | language list; "auto-detection of language is mandatory" | yes |
| Age rating | — | yes |
| Cloud save option | enable only if the game uses the Player data methods | — |

The material rules that decide whether those files are accepted, from S1:

- **5.1.1** materials use elements from the game itself or directly related to
  it; **5.1.1.1** gameplay from other games is prohibited, and so are identical
  promotional materials across different games.
- **5.1.1.2** "Screenshots must showcase real gameplay of the game, which should
  occupy no less than 70% of the image", with an exception for materials that
  effectively demonstrate the core mechanics and graphics.
- **5.1.1.3** the same 70% rule applied to the **duration** of non-promotional
  video.
- **5.1.3** the name is identical across the game and every draft material, per
  declared language; **5.12** the catalogue name is unique across all declared
  languages.
- **5.2** every required field must be completed; **5.3** text fields and files
  must comply with the content, length, size, format and duration standards
  "specified in the draft form" — the form is named as the authority inside the
  requirement itself.
- **5.4** tags and keywords match the topic and content; **5.11** repeating
  symbols such as spaces and hyphens can't be used to reach a text field's
  minimum length.
- **5.6** "Screenshots from the game can't be used as icons/covers."
- **5.9** black bars at the sides or top and bottom are allowed only if they are
  an integral part of the game.
- **8.3.1** good quality, no pixel-compression artefacts, no excessive shadows,
  no monochrome frames, no truncated text in icons or covers.
- **8.3.2** the media reflects what the object is, not a random image or video.
- **8.3.3** "The content has no borders or rounded corners."
- **8.3.4** no system interface elements and no Yandex Games interface elements;
  the game's own interface is permitted **except** on icons and covers.
- **8.2.1-8.2.5** texts follow the grammar and punctuation of the selected
  language, reflect the actual mechanics without misleading, are translated where
  they matter for gameplay, carry no profanity in any language, and contain no
  prohibited material.
- **6.5 / 6.6** (recommendations) the name is short and does not contain the word
  "game" unless it is an integral part of the name.
- **6.1** (recommendation) the developer's or publisher's email address is
  provided.

### What the docs do not decide, and the console does

- **[docs silent]** on a maximum screenshot count. S14 gives a floor ("at least 2
  screenshots for each selected platform") and no ceiling. Read the ceiling off
  the form; do not assume five.
- **[docs silent]** on whether promotional materials are stored per declared
  language and mirrored between languages. **[console]** The studio has observed
  the draft keeping its own screenshot and video set per language, mirroring the
  first language's files into the others until the per-field checkbox is cleared
  — the default outcome being a Russian card showing an English HUD. No
  documentation page states this; treat it as an observation to re-verify on the
  live form, not as a documented rule.
- **[console]** The studio has observed that the draft could not be submitted
  without a horizontal video. S14 marks both video fields **optional** (no
  required marker, unlike Archive, Platforms, Orientation, Languages, Age rating,
  Categories, Title, Description, How to play, Icon, Cover and Screenshots).
  These two statements conflict; the form is the authority, and this row should
  be settled on the next real draft rather than carried forward as fact.
- **[console]** Video slots cannot be filled by a browser agent. The archive and
  screenshot widgets ship a file to the server, but a video widget first reads
  the clip's metadata through a `<video>` element on a blob URL, and a file the
  browser did not pick through its own dialog stays at `readyState 0` forever —
  the handler returns with no error, no message and no request. The draft JSON
  route (`PATCH /console/api/application-draft/<id>` with file ids from
  `POST /console/api/files/{screenshots,videos}`) does not help for video either:
  `options.orientation` is written by the widget, cannot be passed to the upload,
  and a clip lacking it is never placed in a slot. Plan for a person to drag the
  mp4 files in. **[docs silent]** — none of this is documented; it is observed
  console behaviour.
- **[docs silent]** on which text fields may repeat one another. 5.11 bans
  padding a field with repeating symbols; nothing on the live list bans one field
  reusing another field's text. Do not cite a requirement number for that.

---

## 6. Submission funnel

### 6.1 The path

1. Register, create the game, fill the draft (S20, S14).
2. Test the draft before submitting: draft mode puts the files on Yandex's server
   with full platform interaction, "at the stage of final game verification
   before submission for moderation" (S19). Before that, the dev proxy (S12) and
   the debug panel (S13) answer the SDK requirements without an account.
3. Submit for moderation.
4. **Full moderation: 3-5 business days** — first publication, and any update
   where the game files changed. **Content moderation: 1-2 business days** — when
   only promotional materials changed (S15).
5. On rejection a moderator's comment appears in the console and an email names
   the violated requirements with attached evidence; the draft goes to *Rejected*
   (S15).

### 6.2 The doubling cooldown is the real cost of a rejection

"The number of attempts is unlimited, but the waiting time before resubmitting
for moderation doubles after each rejection" — from 24 hours after the first
rejection up to a maximum of 384 hours, 16 days (S15). A "Moderation cooldown
reset" button is available **once per 28 days** (S15).

So the expensive thing is not the review, which is a week at most; it is the
third and fourth rejection. Every requirement answered from memory buys a longer
wait than the check that would have settled it.

### 6.3 The automated pre-check

**[console]** The console runs its own automated check on an uploaded archive and
prints the verdict above the form ("Замечания к релизу"): free, the same check
moderation starts from, and it answers in minutes — so the loop is upload, read,
fix, upload. **[docs silent]**: neither S15, S19 nor S20 mentions an automatic
pre-check; S15 frames pre-submission testing as the developer's own job with the
debug panel and a device sweep. Treat the pre-check as observed console
behaviour.

### 6.4 Categories and the contract

- **Categories: no more than two** (S14). This one is documented, not folklore.
- **The contract.** S16: for a Russian legal entity or sole proprietor "ads are
  enabled automatically together with the contract, no need to register with
  YAN"; otherwise monetization is enabled by registering through the console
  (*Enable monetization* → form → terms → *Register*), after which "you can
  upload new games and earn money", and "Ad blocks will be created after the game
  is published provided that successful registration in YAN has occurred". S17
  describes the unified licensing model that merges internal YAN advertising,
  third-party advertising and in-app purchases into one agreement, signed
  directly by Russian entities and rolled out to others by email.
  **[docs silent]** — neither page states that a draft cannot be *submitted*
  without a live contract. **[console]** The studio has observed the console
  refusing submission without one. The documented consequence is narrower: no
  contract means no ad blocks after publication, which then collides with
  requirement 1.12.
- **1.12 has an escape hatch** (S3): a game may decline monetization entirely by
  saying so in the *Developer's comment* field, and still pass.

### 6.5 Rejection reasons Yandex names itself

From S15, in the platform's own list: context menus appearing on interaction; use
of the system player instead of a built-in one; audio continuing during ads;
sound persisting when switching tabs; improper Game Ready implementation; missing
automatic language detection; missing or incorrect SDK integration; unsaved
progress; game launch failures; console errors; performance issues; and gameplay
that does not match the declared genre.

Six of those twelve are SDK-contract defects (Game Ready, language detection, SDK
integration, saves, ad audio, tab audio) — the same ground as §1 and §4 of this
packet.

---

## 7. What differs sharply from Poki, Playgama and CrazyGames

1. **The moderator's instrument is a panel of coloured lights, and it is public.**
   Yandex documents exactly what the reviewer looks at (S4, S13): loader `IT`/`IF`,
   Game Ready purple→green within 90 s, the language 文 indicator, the gamepad's
   colour. No other portal in this set publishes its reviewer's checklist as a UI
   we can run ourselves. Every SDK requirement is provable before submission.
2. **Language detection is a requirement about a *read*, not about the result**
   (2.14, S5). Poki, Playgama and CrazyGames ask the game to *use* the locale;
   Yandex checks that `environment.i18n.lang` is touched during load, in every
   game, "even if they declare only one language or have no text".
3. **The platform drives gameplay state itself.** On `game_api_pause` the
   platform calls `GameplayAPI.stop()` and on resume `start()` (S8) — and skips
   the restart if the game had already stopped. CrazyGames has no pause callback
   at all, and Playgama aggregates its own pause sources; here the portal is an
   actor in our state machine, not just a notifier.
4. **Interstitial frequency is entirely the platform's** (S9): no cooldown to
   respect, no `adCooldown` error code, and `onClose(wasShown=false)` is the
   documented result of asking too often. Contrast CrazyGames' explicit
   3-minute rule and Playgama's configurable `minimumDelayBetweenInterstitial`.
5. **The sticky banner is drawn by the portal.** No DOM container, no size list,
   no refresh budget — just `showBannerAdv()` / `hideBannerAdv()` and two reason
   codes (S9). The CrazyGames banner model (a visible DOM element with an id,
   30 s cooldown, 120 refreshes) does not transfer in either direction.
6. **Storage is a quota, not a byte cap.** 200 KB per player *and* 100 writes per
   5 minutes (S10), versus CrazyGames' 1 MB with a debounce and Playgama's
   "Bridge picks the place". A per-mutation save that is merely wasteful
   elsewhere is rate-limited here.
7. **The archive ceiling is 100 MB uncompressed** (1.21, S14) — against
   CrazyGames' 250 MB / 1500 files and Playgama's 300 MB. Yandex is the tightest
   of the three, and the limit is on the *uncompressed* content.
8. **Sitelocking is forbidden** (1.18), where CrazyGames documents a domain
   whitelist and a CSP as a recommended defence.
9. **The rejection cost is a doubling cooldown**, 24 h → 384 h (S15). Poki and
   CrazyGames re-review; Playgama re-moderates in 1-5 days. Only Yandex makes the
   *next* attempt more expensive than the last.
10. **Monetization is a requirement** (1.12), not an opportunity — satisfied by
    ads, by purchases, or by an explicit written refusal in the console (S3).
11. **The game keeps a requirement after release**: rating above 30, or the game
    is unpublished after three weeks (2.13). No other portal here ties continued
    publication to a catalogue score in its requirement list.
12. **AI content is split by kind** (1.23): pre-generated assets are allowed,
    interactive AI inside the game is prohibited. Playgama's rule is about
    quality ("low-quality titles built wholly with generative AI"); Yandex's is
    about runtime behaviour.
13. **Most store numbers are documented** (S14) — icon 512×512 PNG, cover
    800×470 PNG, screenshots 1280-2560 px on the long side, video ≤28 s / ≤100 MB
    — where Playgama publishes none of them and CrazyGames publishes covers but
    no formats. For Yandex, `ai_studio/store_kit/yandex_spec.json` can be
    doc-backed rather than form-scraped.

---

## Not covered by Yandex's docs (do not invent a requirement)

- The maximum number of screenshots per platform. Only a floor of 2 is published
  (S14).
- Whether promotional materials are stored and mirrored per declared language.
  **[console]** observation only.
- Whether the horizontal video is genuinely required for submission. S14 marks it
  optional; the console was observed to disagree.
- The automated pre-check above the draft form ("Замечания к релизу"). Observed,
  undocumented (S15, S19, S20 are silent).
- Whether a signed contract blocks *submission*, as opposed to blocking ad blocks
  after publication (S16, S17).
- Any numeric interval between fullscreen ads. "The frequency ... is controlled by
  Yandex Games" is the whole of it (S9).
- Whether `game_api_pause` / `game_api_resume` are dispatched as `window` events
  in addition to `ysdk.on()` (S8 documents only the SDK methods).
- A loading-progress API, a `game_ready` call distinct from `LoadingAPI.ready()`,
  and an SDK analytics/measure sink — none exist in the SDK pages read (S6-S11).
- A pinch-zoom requirement number. The list has none; 1.10.x and the full-screen
  items are what a zoomed page violates.
- Revenue share percentages, payout minimums and payout schedule — not in the
  pages read; the console and the contract carry them.

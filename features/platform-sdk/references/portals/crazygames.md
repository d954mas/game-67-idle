---
type: Reference Lesson
title: CrazyGames Release Guidelines (research packet)
description: CrazyGames publishing requirements, SDK v3 contract, QA gates and rejection risks, checked against primary sources, mapped onto this repo's platform-sdk wrapper contract.
tags: [game-knowledge, crazygames, web-release, sdk, qa]
status: research
sources_checked: 2026-09-07
---

# CrazyGames release guidelines — primary-source packet

All claims below come from CrazyGames' own developer documentation
(`docs.crazygames.com`, `developer.crazygames.com`) and from the live SDK
script, checked 2026-09-07. Where a statement is my inference rather than
CrazyGames' text, it is marked **[inference]**. Where the documentation is
silent, this packet says **[docs silent]** instead of guessing.

Source index:

- S1 `https://docs.crazygames.com/` — docs root: Launching on CrazyGames, Basic/Full Launch, Testing your game
- S2 `https://docs.crazygames.com/sdk/intro/` — SDK v3 introduction: loader, `init()`, environment, error format, module table
- S3 `https://docs.crazygames.com/sdk/game/` — Game module: gameplay/loading events, happytime, invites, settings
- S4 `https://docs.crazygames.com/sdk/video-ads/` — `requestAd`, callbacks, error codes, adblock detection
- S5 `https://docs.crazygames.com/sdk/banners/` — banner API, sizes, refresh rules, error codes
- S6 `https://docs.crazygames.com/sdk/data/` — Data module (persistent storage)
- S7 `https://docs.crazygames.com/sdk/user/` — User module, `systemInfo`, locale
- S8 `https://docs.crazygames.com/sdk/in-game-purchases/` — in-game purchases via Xsolla
- S9 `https://docs.crazygames.com/sdk/leaderboards-client/` — Leaderboards SDK
- S10 `https://docs.crazygames.com/requirements/intro/` — Basic vs Full implementation, QA tool
- S11 `https://docs.crazygames.com/requirements/technical/` — technical requirements
- S12 `https://docs.crazygames.com/requirements/gameplay/` — gameplay requirements
- S13 `https://docs.crazygames.com/requirements/ads/` — advertisement requirements
- S14 `https://docs.crazygames.com/requirements/account-integration/` — account/cloud-save requirements
- S15 `https://docs.crazygames.com/requirements/game-covers/` — cover images and preview video
- S16 `https://docs.crazygames.com/requirements/quality/` — quality guidelines (advisory)
- S17 `https://docs.crazygames.com/resources/html5/sitelock/` — sitelock, domain whitelist, CSP `frame-ancestors`
- S18 `https://docs.crazygames.com/resources/html5/common-fixes/` — scroll/keyboard/visibility/context-menu fixes
- S19 `https://docs.crazygames.com/resources/basic-launch-metrics/` — Basic Launch KPI thresholds
- S20 `https://docs.crazygames.com/resources/getting-to-the-first-frame/` — loading strategy, what CrazyGames measures
- S21 `https://docs.crazygames.com/faq/` — FAQ: updates, rejection reasons, revenue, exclusivity, support
- S22 `https://sdk.crazygames.com/crazygames-sdk-v3.js` — the live SDK bundle (read directly; reports version 3.8.0)
- S23 `https://docs.crazygames.com/resources/html5-resources/` — HTML5 resources index (pages split into intro / sitelock / common-fixes)
- S24 `https://developer.crazygames.com/qatool` — Preview / QA tool on the developer portal (URL surfaced by site search; page itself is behind the portal and was not fetched)
- S25 `https://docs.crazygames.com/sdk/html5-v2/intro/` — legacy v2 SDK docs, still published alongside v3
- S26 `https://developer.crazygames.com/submit` — the submission form itself, read field by field while filling a draft

Repo files this packet maps onto: `features/platform-sdk/references/contract.md`,
`features/platform-sdk/web/adapters/yandex.js`,
`features/platform-sdk/web/adapters/playgama.js`,
`features/platform-sdk/publish-targets/*.json`.

---

## 1. SDK integration contract

### 1.1 Loading and init

One script tag in `<head>`, loaded before game code (S2):

```html
<script src="https://sdk.crazygames.com/crazygames-sdk-v3.js"></script>
```

S2 states explicitly: do **not** bundle or self-host the SDK; use the CDN link.
The live bundle at that URL reports version **3.8.0** and defines the global
`window.CrazyGames.SDK` (S22). There is no npm package in the documentation
(S2) — **[docs silent]** on any package-manager distribution.

Initialization is mandatory and asynchronous (S2):

```javascript
try {
    await window.CrazyGames.SDK.init();
} catch (e) {
    console.log("Init error: ", e);
}
```

or

```javascript
window.CrazyGames.SDK.init()
    .then(() => console.log("Initialized"))
    .catch((e) => console.log("Init error: ", e));
```

"The v3 SDK requires initialization before being used" and it must be awaited;
call it "before the game starts, for example on the loading screen" (S2). The
SDK is promise-based and "doesn't accept a `callback` parameter" (S2) — with
the one exception of `requestAd`, which is callback-based (see 1.4).

### 1.2 Environment

`window.CrazyGames.SDK.environment` returns one of three values (S2):

| Value | When | Behavior |
| --- | --- | --- |
| `local` | localhost / 127.0.0.1 | demo ads, simulated behavior |
| `crazygames` | CrazyGames.com domains | full functionality |
| `disabled` | any other domain | **all SDK calls throw errors** |

This is the sharpest operational difference from Poki/Yandex/Playgama: on an
unknown host the CrazyGames SDK does not degrade to no-ops, it throws (S2).

### 1.3 Modules

`ad`, `banner`, `game`, `user`, `data`, plus in-game purchases (S2). The live
bundle also exposes `analytics` and `store` namespaces (S22).

Error objects everywhere have the shape (S2):

```javascript
{ code: 'userAlreadySignedIn', message: 'The user is already signed in' }
```

### 1.4 Game module (S3)

```javascript
window.CrazyGames.SDK.game.gameplayStart();
window.CrazyGames.SDK.game.gameplayStop();
window.CrazyGames.SDK.game.loadingStart();
window.CrazyGames.SDK.game.loadingStop();
window.CrazyGames.SDK.game.happytime();
window.CrazyGames.SDK.game.reportGameCompletedPercentage(50);
```

- `gameplayStart()` — "called whenever the player starts playing or resumes
  playing after a break": game start, resume, revive, level entry (S3).
- `gameplayStop()` — "called on every game break (entering a menu, ending
  level, pausing the game)". Explicitly **not** to be called when the user
  switches focus or leaves the game area (S3). Our contract calls
  `gameplay_stop()` on the same set of breaks, so the semantics line up, but
  the "don't stop on focus loss" rule is a CrazyGames-specific constraint on
  any pause/visibility handler.
- `loadingStart()` / `loadingStop()` bound the loading interval. There is no
  progress-fraction API — **[docs silent]** on anything equivalent to Poki
  `gameLoadingProgress`.
- `happytime()` triggers a site-level confetti celebration; "use this feature
  sparingly, the celebration should remain a special moment" — not for routine
  level completion or item acquisition (S3). It has no counterpart in our
  wrapper contract.
- `setGameContext(obj)` / `clearGameContext()` attach in-game data (level,
  equipped items, currency) to user feedback reports (S3).
- Multiplayer/invite surface: `updateRoom({roomId, isJoinable, inviteParams})`,
  `leftRoom()`, `addJoinRoomListener` / `removeJoinRoomListener`,
  `inviteLink(paramMap)`, `getInviteParam(name)`, `inviteParams`,
  `isInstantMultiplayer` (S3). `showInviteButton` / `hideInviteButton` are
  deprecated in favor of room data (S3).
- `game.settings` carries `disableChat` and `muteAudio`, with
  `addSettingsChangeListener` / `removeSettingsChangeListener` (S3). This is the
  closest thing CrazyGames has to a portal-driven audio callback; it is a
  settings object, not a pause/resume event.

There is **no** portal pause/resume callback pair equivalent to Yandex
`game_api_pause` / `game_api_resume` — **[docs silent]**.

### 1.5 Ads (S4, S13)

Callback-based, single entry point:

```javascript
const callbacks = {
  adFinished: () => console.log("End midgame ad"),
  adError: (error) => console.log("Error midgame ad", error),
  adStarted: () => console.log("Start midgame ad"),
};
window.CrazyGames.SDK.ad.requestAd("midgame", callbacks);
// or
window.CrazyGames.SDK.ad.requestAd("rewarded", callbacks);
```

Error codes (S4):

| Code | Meaning |
| --- | --- |
| `adsDisabledBasicLaunch` | during Basic Launch ads are disabled |
| `unfilled` | no ad available |
| `adblock` | an adblocker prevents showing ads |
| `adCooldown` | ad was requested too soon (usual midgame interval: 3 minutes) |
| `other` | unspecified error |

Adblock probe (S4):

```javascript
const result = await window.CrazyGames.SDK.ad.hasAdblock();
```

Rules:

- "Make sure to mute the audio and pause the game when the ad starts
  (`adStarted` callback), and to unmute the audio and continue the game when the
  ad finishes/fails to load (`adError` and `adFinished` callbacks)" (S4).
  S13 refines this: mute only when the ad actually begins playing, not on
  request; while the request is in flight, pause gameplay and "disable buttons,
  or show a spinner that blocks interaction".
- Midgame frequency is handled by the SDK: "request midgame ads at opportune
  moments without worrying about frequency", max 1 every 3 minutes (S13).
  Request at logical transitions (level change, map shift, player death); do
  **not** attach ads to navigational buttons, settings, or shop openings (S13).
- Rewarded: reward only on `adFinished`; never reward on `adError` (S13). Ad
  chaining (several ads for one reward) is forbidden; rewarded offers must be
  outside active gameplay screens, in a consistent accessible location, with a
  video icon, and there should be a non-ad alternative (S13).
- "Only Ads requested through the CrazyGames SDK are allowed" — third-party ads
  are forbidden (S13).
- There is no `wasShown` boolean and no distinct "skipped" outcome; the only
  outcomes are `adStarted`, `adFinished`, `adError(code)` (S4). Neither S4 nor
  S22's documented surface exposes a "user closed early" signal —
  **[docs silent]**.
- The live bundle also contains `prefetchAd()` (S22), which the public v3 ads
  page does not document (S4).

### 1.6 Banners (S5)

```javascript
await window.CrazyGames.SDK.banner.requestBanner({ id: "banner-container", width: 300, height: 250 });
await window.CrazyGames.SDK.banner.requestResponsiveBanner("responsive-banner-container");
window.CrazyGames.SDK.banner.clearBanner("banner-container");
window.CrazyGames.SDK.banner.clearAllBanners();
```

Static sizes: 728x90, 300x250, 320x50, 468x60, 320x100. Responsive containers
accept 970x90, 320x50, 160x600, 336x280, 728x90, 300x600, 468x60, 970x250,
300x250, 250x250, 120x600 — "only banners that fit into your container will be
displayed" (S5).

Constraints: minimum 30 s between refreshes of the same container; maximum 120
refreshes per session per banner size; no banners during video ad playback;
containers must be fully visible (S5). Error codes include `bannerCooldown`,
`unfilled`, `notVisible`, `videoAdPlaying`, `invalidSize`, `maxRefreshReached`,
`bannersDisabledMobileApp`, `noAvailableSizes` (S5). Maximum 2 banners per
screen and never during active play (S13).

Banners require a **DOM element** with an id (S5). A full-canvas WASM game has
no such element unless the shell creates one, so this is not a drop-in for the
Yandex "sticky banner drawn by the portal" model.

### 1.7 Data / storage (S6)

```javascript
window.CrazyGames.SDK.data.getItem(key);      // string | null
window.CrazyGames.SDK.data.setItem(key, value);
window.CrazyGames.SDK.data.removeItem(key);
window.CrazyGames.SDK.data.clear();
```

- Same API as `localStorage`, designed as a drop-in replacement (S6).
- **Synchronous** calls; writes are debounced — "multiple calls to the methods
  will be saved after 1 second", with exceptions extending to 30 seconds (S6).
- Hard cap 1 MB: "Game data when converted to a JSON string cannot exceed
  1048576 bytes", error code `dataLimitExcedeed` (S6, spelling as published).
- Guests are stored in `localStorage`; on login the SDK loads the account data
  if any exists, or transfers the guest data into the account (S6).

### 1.8 User / locale (S7)

```javascript
const user = await window.CrazyGames.SDK.user.getUser();          // or null
const user2 = await window.CrazyGames.SDK.user.showAuthPrompt();
const available = window.CrazyGames.SDK.user.isUserAccountAvailable;
const token = await window.CrazyGames.SDK.user.getUserToken();
window.CrazyGames.SDK.user.addAuthListener(listener);
const systemInfo = window.CrazyGames.SDK.user.systemInfo;
```

`systemInfo` (S7):

```javascript
{
    "countryCode": "US",
    "locale": "en-US",
    "device": { "type": "desktop" },        // "desktop" | "tablet" | "mobile"
    "os": { "name": "Windows", "version": "10" },
    "browser": { "name": "Chrome", "version": "107.0.0.0" },
    "applicationType": "web"                 // "google_play_store" | "apple_store" | "pwa" | "web"
}
```

"If you want to automatically set the language of the game based on user
location, please use the **locale** field for this" (S7). `systemInfo` is a
synchronous property, not a promise (S7). This is our `getLocale()` source.

The user object exposes `__dangerousUserId`, `username`, `profilePictureUrl`
(S7). `getUserToken()` returns a JWT with `userId`, `gameId`, `username`,
`profilePictureUrl` for server-side verification (S7).

### 1.9 Analytics

There is **no** portal analytics/event API comparable to `PokiSDK.measure()`.
The only analytics call in the documentation is
`window.CrazyGames.SDK.analytics.trackOrder("xsolla", order)` for purchase
orders (S8). Third-party analytics are not discussed on the technical
requirements page — **[docs silent]** on whether a game may ship its own
analytics endpoint; the ads page only forbids third-party *ads* (S13).

### 1.10 In-game purchases (S8)

Invite-only. There is no `requestInAppPurchase` in the v3 SDK; CrazyGames issues
an Xsolla token (`await window.CrazyGames.SDK.user.getXsollaUserToken()`) and the
purchase itself runs through Xsolla's own SDK, with optional
`analytics.trackOrder` afterwards (S8). Guests cannot purchase (S8). Not
relevant to a first CrazyGames port. **[inference]** Our contract has no IAP
surface, so nothing to map.

### 1.11 Leaderboards (S9)

`submitScore({ encryptedScore, score })`, where the score is AES-GCM encrypted
client-side with `encryptScore(score, encryptionKey)` before submission (S9).
The live bundle exposes `submitScore` and `listFriends` on the user module
(S22). **[docs silent]** on whether a signed-in user is required.

---

## 2. Mapping onto this repo's wrapper contract

Adapter file to add: `features/platform-sdk/web/adapters/crazygames.js`,
exporting `createCrazygamesPlatformAdapter({ host })` and the
`createPlatformSdkAdapter` alias, matching `yandex.js` / `playgama.js`.

The adapter object our facade expects (read off `yandex.js` lines 207-226 and
`playgama.js` lines 193-214) is exactly: `destroy`, `gameLoadingProgress`,
`gameLoadingFinished`, `gameReady`, `gameplayStart`, `gameplayStop`,
`getLocale`, `hideBanner`, `loadData`, `measure`, `ready`, `saveData`,
`showBanner`, `showInterstitial`, `showRewarded`.

| Adapter method | CrazyGames implementation | Source |
| --- | --- | --- |
| `ready()` | inject `https://sdk.crazygames.com/crazygames-sdk-v3.js`, `await window.CrazyGames.SDK.init()`, treat `environment === "disabled"` as not ready | S2 |
| `gameLoadingProgress(p)` | no-op; call `game.loadingStart()` once at adapter init instead | S3 |
| `gameLoadingFinished()` | `game.loadingStop()` | S3 |
| `gameReady()` | no-op — no `game_ready` equivalent | S3, **[docs silent]** |
| `gameplayStart()` | `game.gameplayStart()` | S3 |
| `gameplayStop()` | `game.gameplayStop()` | S3 |
| `showInterstitial(placement)` | `ad.requestAd("midgame", {adStarted, adFinished, adError})`; resolve `{supported:true, shown:true}` on `adFinished` after `adStarted`, `{shown:false, reason:<error.code>}` on `adError` | S4 |
| `showRewarded(placement)` | `ad.requestAd("rewarded", …)`; `rewarded:true` only on `adFinished`; never on `adError` | S4, S13 |
| `showBanner()` | `banner.requestResponsiveBanner(containerId)` **only if** the web shell owns a visible DOM container; otherwise `{supported:false, shown:false, reason:"unsupported"}` | S5 |
| `hideBanner()` | `banner.clearAllBanners()` (or `clearBanner(id)`) | S5 |
| `loadData(key)` | `data.getItem(key)` (sync string), `JSON.parse` guarded | S6 |
| `saveData(key, value)` | `data.setItem(key, JSON.stringify(value))`, 1 MB budget | S6 |
| `getLocale()` | `user.systemInfo.locale`, fallback `navigator.language` | S7 |
| `measure()` | no-op — no portal event API | S8, **[docs silent]** |
| `destroy()` | cancel pending ad promises like `playgama.js` does | repo |

Notes for the implementer:

- `placement` from our contract has no CrazyGames counterpart: `requestAd` takes
  only `"midgame"` / `"rewarded"` (S4). Keep the placement in our own event
  payload only.
- CrazyGames' ad callbacks are not promises. Wrap them in the same
  `adOperation(start, failedResult)` timeout helper the Yandex and Playgama
  adapters use, so a portal that never calls back still settles.
- On `environment === "disabled"` every call throws (S2). Every SDK touch in
  the adapter must be inside `try/catch`, and `ready()` must return false rather
  than letting the throw escape. **[inference]** For a target build this is a
  boot failure by our contract's rule (`PLATFORM_SDK_BOOT_FAILED`), which is
  correct: a crazygames-target artifact served from a non-CrazyGames host is
  misconfigured.
- `data` is synchronous; our `loadData`/`saveData` are async. Wrapping a
  synchronous call in an async function is fine, but do not assume a write has
  reached the cloud on return — debounce is 1 s and can stretch to 30 s (S6).
- A `publish-targets/crazygames.json` manifest is needed with
  `sdk_url: "https://sdk.crazygames.com/crazygames-sdk-v3.js"` and
  `forbidden_markers` covering the Poki/Yandex/Playgama SDK URLs, matching the
  shape of `publish-targets/playgama.json`.

---

## 3. Hard submission requirements

### 3.1 Size and file layout (S11)

- Total build: **250 MB maximum**, **1500 files maximum**.
- Initial download: **≤ 50 MB**; **≤ 20 MB** to be eligible for the mobile
  homepage. The measured window ends at the SDK `Gameplay start` event (S11,
  S20).
- "Use only relative paths when referring to other files in the game bundle.
  Never use absolute paths" (S11).
- **[docs silent]** in S11/S21 on the archive format and the mandatory entry
  filename. S21 only says "simply upload the updated files and submit them for
  approval". Third-party guides state a ZIP with `index.html` at the root, but
  that is not CrazyGames' own text — treat it as unconfirmed and verify in the
  developer portal upload form (S24).

### 3.2 Devices, viewports, browsers (S11, S12)

- Chrome and Edge required; Safari support varies. Chromebook support required
  (smooth at 4 GB RAM) (S11).
- Desktop must be playable in landscape; portrait is allowed with letterboxing;
  mobile orientations are configured in the submission settings (S11).
- Content must be readable at `devicePixelRatio: 1` in 16:9 iframes at, among
  others, desktop non-fullscreen 907x510, 1216x684, 1077x606, 821x462; desktop
  fullscreen 1366x768, 1920x1080, 1536x864, 1280x720; mobile 800x450; tablet
  1080x607 (S12).
- Physics must stay consistent across 144 Hz / 165 Hz monitors (S12).
- Recommended CSS to suppress selection UI on mobile (S11):

```css
-webkit-user-select: none;
-moz-user-select: none;
-ms-user-select: none;
user-select: none;
```

- Device Pixel Ratio is managed by the platform (DPR = 1 on iOS and low-memory
  Android) (S11).
- iOS suspends the AudioContext in the background; resume it from a user gesture
  (S11):

```javascript
document.addEventListener("touchend", () => {
    if (audioContext && audioContext.state === "suspended") {
        audioContext.resume();
    }
});
```

- **[docs silent]** on a general "mute audio when the tab loses focus" rule.
  S18's common-fixes snippet handles `visibilitychange` (for the Samsung app
  webview), wheel-scroll, arrow/space key defaults and the context menu, but
  does not prescribe muting on blur. S4/S13 require muting only around ads.

### 3.3 Content and UX rules (S12, S13)

- "Fullscreen mode is automatically provided by CrazyGames. Custom in-game
  fullscreen buttons are prohibited" (S12).
- "The game should not include cross-promotions for external or internal
  games/platforms" (S12). Narrow exceptions: privacy/terms links, community
  links (Discord, developer site) **on menus only**, store links for desktop
  versions, backlinks to CrazyGames, and references to a game's own sequels
  (S12).
- Games must "land new users in gameplay immediately. If this is not feasible
  given the game specifics, a maximum of 1 click is allowed" (S12).
- English localization is mandatory; other languages should be accurate and
  chosen from the SDK's system-info locale, defaulting to English (S12).
- PEGI 12 compliance, audience 13+ (S12).
- Additional personal-data collection requires a Terms/Privacy notice for new
  players, preferably non-blocking (S11).
- **[docs silent]** on splash screens or logos of other portals as a named rule;
  the applicable text is the cross-promotion ban above (S12) plus the cover-art
  ban on store logos (S15).

### 3.4 Sitelock and embedding (S17)

Sitelock is optional but, if implemented, must whitelist every CrazyGames
domain (S11, S17):

```javascript
function isCrazyGames() {
    const hostname = window.location.hostname;
    const parts = hostname.split(".");
    const idx = parts.indexOf("crazygames");
    return idx !== -1 && idx >= parts.length - 3;
}
```

CSP for iframe protection (S17):

```
Content-Security-Policy: frame-ancestors 'self' *.crazygames.com https://app.crazygames.com capacitor://app.crazygames.com;
```

Whitelist patterns `*.crazygames.com` and `crazygames.*`, with the published
regional list including www/de/it/vn/gr/ar/th `.crazygames.com` and
`www.crazygames.` fr, co.id, cz, dk, hu, nl, no, pl, com.br, ro, fi, se, ru,
com.ua, at, jp, pt, vn, com.vn, co.kr, plus `games.crazygames.com`,
`https://app.crazygames.com`, `capacitor://app.crazygames.com` (S17).

**[docs silent]** on an outbound-request allowlist or a CSP imposed on the game
itself — CrazyGames publishes no equivalent of Poki's external-resources page.
**[inference]** Our artifact should still ship self-contained, since the sitelock
guidance assumes the bundle is served from CrazyGames' own hosting.

### 3.5 SDK integration levels (S10, S11)

| Level | Required | Ads |
| --- | --- | --- |
| Basic Implementation | `Gameplay start` event from the Game module | disabled |
| Full Implementation | `Gameplay start`/`stop`, Data module (if the game saves progress), User module (if the game has accounts); Load start/stop optional | enabled |

Account integration (S14): progress must sync across devices; the Data module
is required for progress saving unless the game has its own backend; guests must
always be able to play; the auth prompt "must not trigger automatically"; the
login button belongs top-right and must not be the blocking main CTA; prefer the
Data module over raw `localStorage` because "multiple users might share the same
device".

### 3.6 The submission form's own answers (S26)

The developer portal's submit form is the fourth source, and it contradicts the
docs in one place worth knowing before a game is built.

- The build is uploaded as a **folder**, not an archive. The control is a
  directory input and the only stated rule is that `index.html` sits at its
  root.
- The game name is capped at **35 characters** and must be the same string the
  game itself shows.
- An Emscripten/wasm build files under engine **HTML5**. The list also offers
  Externally hosted (iframe), Unity 6, Unity 2022, Godot, Defold, GameMaker,
  GDevelop, Cocos and PlayCanvas.
- The progress-save question offers a fourth answer the docs barely mention:
  **Automatic Progress Save**, where the portal syncs the game's own
  `localStorage` to the player's account. It removes the need for the Data
  module for a game that already persists locally and has no in-game purchases,
  which is the one case S14 rules out. The form states it does not work for
  iframe games.
- A first submission is locked to the **Basic** flow; the Full control beside it
  is a tooltip describing global release, not a choice.
- The steps are Upload, QA, Details, Submit. Covers, videos and listing texts
  live in Details, so none of them can be filled before a build is uploaded.

---

---

## 4. Store / listing assets (S15)

Cover images — all three are mandatory and must share a visual style:

| Format | Ratio | Pixels |
| --- | --- | --- |
| Landscape | 16:9 | 1920x1080 |
| Portrait | 2:3 | 800x1200 |
| Square | 1:1 | 800x800 |

**[docs silent]** on the accepted image file formats and per-image size caps.

Cover rules (S15): no borders; no text other than the game's title (no "New",
"Updated", "Play Now"); no icons or store logos; no copyrighted visuals you do
not own; nothing blurry or pixelated; don't just screenshot the game; keep it
uncluttered; a stylized title font is encouraged.

Preview video (S15): 15-20 seconds (longer is cut to 20 s), **50 MB maximum**,
both landscape 1080p 16:9 **and** portrait 1080p 2:3 are mandatory; no audio; no
black screens, logo transitions, letterbox bars, default cursor, promotional
text, or app/social icons; no manual fast-forwarding (the system applies a small
speed-up); use the static cover as the opening frame. Animated video is
acceptable if it represents real gameplay.

**[docs silent]** on screenshots: the game-covers page specifies covers and a
preview video only, with no screenshot count or size. **[docs silent]** on
description/title character limits and on the list of supported store languages;
the only localization rule found is the gameplay requirement that the game
itself ship English (S12).

---

## 5. Submission funnel

1. Create a developer account and a game on the developer portal, upload the
   build, and use the **Preview tool** (`crazygames.com/preview`, reachable via
   "Submit a game") to run the game as it would run on CrazyGames and to check
   requirements before submitting (S1, S10, S24).
2. QA reviews the submission "according to our technical and quality
   requirements" (S1). **[docs silent]** on the first-review turnaround in the
   official docs; S21 states only that *updates* are "usually processed within
   the same working day", and that Basic Launch updates go live instantly.
3. **Basic Launch**: "Test your game on our platform with a limited audience for
   a temporary period of 7 to 21 days" — basic QA, no CrazyGames-specific
   integration required, monetization disabled (S1). It ends when the game is
   ≥ 7 days live **and** has ≥ 500 plays; if 500 plays are not reached, it ends
   automatically at 21 days (S1, S19).
4. Basic Launch KPIs and their success targets (S19): average play time 10+
   minutes per session; day-1 retention 10-15%; conversion (players who play at
   least one minute after starting) 80%+, with build size under 20 MB and load
   time under 10 seconds.
5. Outcomes (S1): above benchmark → invited to Full Launch (full integration,
   full QA, monetization on, revenue share); partially below → asked to improve
   and relaunch; below → cannot proceed, resubmission requires substantial
   improvement.

Rejection reasons named by CrazyGames (S21): "bugs or broken mechanics";
"missing English-language support"; "unoriginal content (e.g. clones or asset
flips)"; inappropriate themes / PEGI-12 non-compliance; failure to meet the
Developer Requirements or ethical standards. S16 adds the naming/identity rule:
a game must not be "easily confused with another that features a similar name or
iconography", and generic names without owned IP are discouraged.

Commercials (S21): no exclusivity — publishing elsewhere (Steam, App Store,
Google Play) does not affect revenue-share eligibility; earnings vary with
performance; minimum payout €100, monthly, wire or PayPal. **[docs silent]** on
the revenue-share percentage.

---

## 6. What differs sharply from Poki / Yandex / Playgama

These are the places where a naive port of an existing adapter breaks.

1. **`disabled` environment throws.** Poki and Yandex adapters degrade to
   no-ops off-portal; CrazyGames throws on every call from an unknown host
   (S2). Every call site needs a guard.
2. **Ads are callbacks, not promises, and have no "shown" boolean.** Yandex
   gives `onClose(wasShown)`, Playgama gives a state machine; CrazyGames gives
   `adStarted` / `adFinished` / `adError(code)` only (S4). Model "skipped"
   as absent — **[inference]** treat `adFinished` without a preceding
   `adStarted` as not shown, and any `adError` as `shown:false` with
   `reason = error.code`.
3. **`adsDisabledBasicLaunch` is a normal steady state**, not a bug (S4): for
   the first 7-21 days every ad request fails. The game must not gate
   progression behind a rewarded ad that can never fill.
4. **No portal analytics.** `measure()` has no sink (S8) — unlike Poki.
5. **No loading-progress API.** Only `loadingStart` / `loadingStop` (S3), so
   `gameLoadingProgress()` is a no-op and `loadingStart()` must be called
   explicitly at adapter init — nothing calls it for us.
6. **No pause/resume callbacks.** Yandex `game_api_pause` / `game_api_resume`
   and Playgama's runtime state have no counterpart; the nearest signal is
   `game.settings.muteAudio` plus a settings-change listener (S3). Our
   `platform_sdk_on_pause` / `on_resume` listeners will only ever fire from the
   ad flow on this portal.
7. **`gameplayStop()` must not be called on focus loss** (S3). If our game
   layer stops gameplay on `visibilitychange`, that path must be suppressed for
   this target.
8. **Storage is synchronous, `localStorage`-shaped, 1 MB, debounced** (S6) —
   not Yandex's async `player.setData` with its own quota.
9. **Banners need a real DOM container** and are forbidden during active play,
   max 2 per screen (S5, S13). The Yandex "portal draws a sticky banner" model
   does not transfer; **[inference]** report banners as unsupported until the
   web shell grows a container.
10. **Custom fullscreen buttons are prohibited** (S12) — Poki has no such ban.
11. **Community links (Discord) are explicitly allowed on menus** (S12), unlike
    the blanket external-link ban we apply to Poki/Yandex. Our
    `external_links_allowed` capability is a single boolean, so it cannot
    express "menus only"; **[inference]** keep it `false` for this target and
    add the Discord link back only if the lead wants it, rather than widening
    the capability.
12. **Locale is a synchronous property** (`user.systemInfo.locale`), not a
    promise and not read during init (S7) — simpler than the Yandex
    `environment.i18n.lang` timing requirement our adapter comments about.

---

---
type: Reference Lesson
title: Poki Release Guidelines (research packet)
description: Poki publishing requirements, SDK contract, QA gates and rejection risks, checked against primary sources.
tags: [game-knowledge, poki, web-release, sdk, qa]
status: research
sources_checked: 2026-08-24
---

# Poki release guidelines — primary-source packet

All claims below are quoted or paraphrased from Poki's own developer
documentation (`sdk.poki.com`, `developers.poki.com`) and from the live SDK
loader script, checked 2026-08-24. Where a statement is my inference rather
than Poki's text, it is marked **[inference]**.

Source index:

- S1 `https://sdk.poki.com/new-requirements.html` — Requirements (the checklist Developer Support runs)
- S2 `https://sdk.poki.com/requirements.html` — older Requirements page (still live, same core rules)
- S3 `https://sdk.poki.com/sdk-documentation.html` — SDK general info, event definitions, full HTML example
- S4 `https://sdk.poki.com/html5.html` — HTML5 integration, code snippets, final steps
- S5 `https://sdk.poki.com/poki-quality-guidelines.html` — Quality Guidelines, clone-vs-inspired table
- S6 `https://sdk.poki.com/poki-inspector.html` — Poki Inspector QA modules and warnings
- S7 `https://sdk.poki.com/playtesting.html` — Level 2, playtest types
- S8 `https://sdk.poki.com/player-fit.html` — Level 3, hard playtime gates
- S9 `https://sdk.poki.com/web-fit-test.html` — Level 4, CTR / time-on-page / conversion-to-play
- S10 `https://sdk.poki.com/final-review.html` — Level 5
- S11 `https://sdk.poki.com/releaseprocess.html` — technical test / soft release / global release
- S12 `https://sdk.poki.com/external-resources.html` — blocked external requests, CSP exceptions
- S13 `https://sdk.poki.com/deals.html` — contractual minimum performance requirements
- S14 `https://sdk.poki.com/working-with-ai.html` — AI-generated content policy
- S15 `https://sdk.poki.com/trends.html` — trend-driven content, oversaturation
- S16 `https://sdk.poki.com/fullscreen.html` — fullscreen eligibility
- S17 `https://sdk.poki.com/game-events.html` — `PokiSDK.measure()` analytics events
- S18 `https://sdk.poki.com/game-thumbnail.html` — thumbnail specs
- S19 `https://sdk.poki.com/index.html` — Working With Poki (submission funnel, what curation looks at)
- S20 `https://sdk.poki.com/web-fit-help.html` — what to fix when Web Fit fails
- S21 `https://developers.poki.com/guide/easy-access` — loading + onboarding guide (SPA; text extracted from the site bundle)
- S22 `https://developers.poki.com/guide/engagement` — engagement guide (same)
- S23 `https://developers.poki.com/guide/monetization` — rewarded-video guide (same)
- S24 `https://game-cdn.poki.com/scripts/v2/poki-sdk.js` — the live SDK loader shim (read directly)
- S25 `https://developer.chrome.com/blog/autoplay/` — Chrome autoplay policy (not Poki, used for the audio-gesture question)

---

## 1. SDK integration contract

### 1.1 Loading

Script tag in `<head>` (S4):

```html
<script src="https://game-cdn.poki.com/scripts/v2/poki-sdk.js"></script>
```

```javascript
PokiSDK.init().then(() => { /* continue to game */ })
              .catch(() => { /* "Initialized, something went wrong, load game anyway" */ });
PokiSDK.gameLoadingFinished();   // when all assets are loaded
```

`gameLoadingFinished()` — "We need this to track the loading progress of your
game and show your conversion to play." (S3)

The documented public surface is `init`, `gameLoadingFinished`,
`gameplayStart`, `gameplayStop`, `commercialBreak`, `rewardedBreak`,
`setDebug`, `shareableURL`, `getURLParam`, `getDeviceInfo`, `movePill`,
`login` / `getUser` / `getToken` (S3, S4). The loader shim additionally
registers, undocumented on those pages: `gameLoadingStart`,
`gameLoadingProgress`, `gameInteractive`, `roundStart`, `roundEnd`,
`happyTime`, `captureError`, `logError`, `muteAd`, `customEvent`,
`sendHighscore`, `showLeaderboard`, `setPlayerAge`, `openExternalLink`,
`generateScreenshot`, `isAdBlocked`, `measure` (S24). **[inference]** Treat
only the documented ones as contract; the others exist but Poki does not
document them for v2 HTML5, so QA will not expect them.

`PokiSDK.measure(category, what, action)` is the documented custom-analytics
call (S17). Reserved characters `/` and `^` are forbidden in all three
arguments. Special actions: `start` / `complete` / `fail` for progress
funnels, `visible` / `interact` for UI-exposure pairs. Ad playback itself is
tracked automatically — you only measure the in-game placement that led to it.

### 1.2 gameplayStart / gameplayStop — exact semantics

Definitions (S3):

- `gameplayStart()` — "Fires whenever the player starts interacting with the
  game (e.g. when the player starts moving around or clicks to interact with
  something)."
- `gameplayStop()` — "Fires whenever the gameplay halts (e.g. when the game is
  paused, the level is completed or when the player is dead)."

Documented event orders (S3):

| Situation | Order |
| --- | --- |
| Startup | `gameLoadingFinished()` > `gameplayStart()` |
| Dies and restarts | `gameplayStop()` > `commercialBreak()` > `gameplayStart()` |
| Dies and revives | `gameplayStop()` > `rewardedBreak()` > `gameplayStart()` |
| Next level | `gameplayStop()` > `commercialBreak()` > `gameplayStart()` |
| Pause / unpause | `gameplayStop()` > `gameplayStart()` |

Note from S3: "if an ad is fired when gameplay is not interrupted (for example
if the player watches an ad to unlock a character skin), it's not necessary to
fire a `gameplayStop()` or `gameplayStart()` event."

QA rules, verbatim from the requirements checklist (S1):

- "SDK events should not fire twice in succession" — a `gameplayStart()` cannot
  follow another `gameplayStart()`; a `gameplayStop()` cannot follow another
  `gameplayStop()`.
- "`gameplayStart()` must fire on the player's first input (not on load)."
- "`gameplayStop()` must fire on any gameplay interruption (pause, menu open,
  level end, cutscene)."
- "It should not be possible to fire any SDK events during midrolls or rewarded
  videos."
- "`commercialBreak()` must fire only when exiting a pause and heading back into
  gameplay" — closing a pause menu into gameplay is correct; leaving gameplay
  into a level-selection screen is wrong.
- Edge case Poki names: dress-up games may fire `gameplayStop()` +
  `commercialBreak()` when swapping clothing categories.

So: **a settings or pause menu is a `gameplayStop()`; resuming from it is
`commercialBreak()` then `gameplayStart()`.** Opening a menu that leads *away*
from gameplay is a `gameplayStop()` with no commercial break at that moment.

### 1.3 commercialBreak — placement and audio

- "Commercial breaks are used to display video ads and should be triggered on
  natural breaks in your game. Throughout the rest of your game, we recommend
  you implement the `commercialBreak()` before every `gameplayStart()`, i.e.
  whenever the user has shown an intent to continue playing." (S4)
- "Not every single `commercialBreak()` will trigger an ad. Poki's system will
  determine when a user is ready for another ad, so feel free to signal as many
  commercial break opportunities as possible." (S4)
- "Do not implement internal ad timers — rely on Poki's system to manage ad
  frequency." (S1)
- Audio and input: "Make sure that audio and keyboard input are disabled during
  commercialBreaks, so that the game doesn't interfere with the ad." (S4)
  Requirement form: "Audio Management: Automatically mute game audio during
  advertisement playback." (S1)
- The callback passed to `commercialBreak(cb)` fires only when an ad actually
  starts; the docs warn "keep in mind that the function above to pause it might
  not always get called", so unmuting must happen in the `.then()`, not only in
  a paired callback. (S4)

Shape (S4):

```javascript
PokiSDK.commercialBreak(() => { /* mute + pause here */ })
       .then(() => { /* unmute, re-enable input */ PokiSDK.gameplayStart(); });
```

### 1.4 rewardedBreak

- Player-initiated only, in exchange for a benefit; "please make it clear to the
  player beforehand that they're about to watch an ad." (S4)
- Resolves with a boolean: reward only when `success === true`. (S4)
- Optional params `{ size: 'small'|'medium'|'large', onStart }`; size can make
  Poki show more rewarded ads. (S4)
- "`rewardedBreak()` affects the timing of `commercialBreak()` — when a user
  interacts with a rewarded break, our system's ad timer is reset." (S4)
- UI rules are hard QA items (S1): there must always be a standard continue
  button next to the rewarded one; both visible simultaneously; the standard
  button is equal or larger and **green**; rewarded buttons may not be green and
  must carry a prominent 🎬/🎞️ icon; one video per reward (no chains); clear
  reward confirmation (animation/sound); apply the reward immediately where
  possible; no double-rewarding; "when ad blockers are detected, do not follow
  through and provide rewards"; and "avoid displaying custom 'ad blocked'
  messages — Poki manages this communication."
- Rewarded videos "should never block content of the core gameplay" — they are
  an optional perk (S23).
- Infinite rewarded views are allowed but you own the balance consequences (S23).

### 1.5 What Poki QA checks for SDK correctness

The Poki Inspector (S6) is the QA tool; Developer Support runs the same list:

- an **Event Log** of every SDK event fired, checked against the rules in 1.2;
- **Game Details** panel with measured **loading time and file size**;
- **Scaling Tests** across dimensions and popular devices, plus a Desktop/Mobile
  toggle (mobile via QR code onto a real phone);
- **Warnings**: External Resources, Image Optimization ("images ... could be
  compressed further"), and "Unexpected Behavior Detected — functionalities that
  might disrupt the expected functionality of the Poki SDK";
- a left-hand **QA Module checklist** the developer ticks off before submitting.

Correct SDK events are a hard prerequisite: "Implement SDK events correctly
(required for Web Fit testing and all releases)" (S1); the Web Fit Test's
conversion-to-play metric is literally "how many players passed the first
`gameplayStart()` event" (S9).

---

## 2. When the SDK fails to load (adblock / offline)

Poki's stated rules:

- "Your game should be playable even if the player has an ad blocker extension,
  you shouldn't withhold any core gameplay behind an ad block message. We deal
  with this condition on the platform side so there is no need to include any
  specific messaging to inform the player." (S1, S2)
- `init()` has a documented `.catch()` branch whose comment is "Initialized,
  something went wrong, load you game anyway" (S4).
- The Unity example gates ad calls behind `adsBlocked()` and calls `StartGame()`
  directly when ads are blocked (S3). The HTML5 example does not — it relies on
  `commercialBreak()` resolving.
- No rewards may be granted when ads are blocked, and no custom "ad blocked"
  message may be shown (S1).

So: **a no-SDK fallback is required in behaviour** (the game must start and be
fully playable), even though Poki does not prescribe an implementation.

**[inference], from reading S24:** the v2 script is a thin shim that queues
calls and then loads the real core (`poki-sdk-core-<hash>.js`) from
`game-cdn.poki.com`. Two distinct failure modes follow, and the documented
`.catch()` covers neither cleanly:

1. `poki-sdk.js` itself is blocked → `window.PokiSDK` is undefined → any direct
   call throws a `ReferenceError`/`TypeError` before your `.catch()` exists.
   Guard with a `typeof PokiSDK === 'undefined'` stub.
2. The shim loads but the core script is blocked → queued calls sit in the queue
   forever. `init()` and `commercialBreak()` return promises that **never settle**
   (neither resolve nor reject), so a startup flow written as
   `init().then(startGame)` hangs on a black screen. `rewardedBreak()` is the
   exception: the shim resolves it `false` immediately.

Practical rule for us: wrap `init()` and every `commercialBreak()` in a timeout
(e.g. `Promise.race` with a 2–3 s timer) that falls through to gameplay, and
never make asset loading or the first frame depend on an SDK promise. This also
covers "must work offline in the Inspector / local file test".

---

## 3. Technical requirements

### 3.1 Size and loading

- "Target an initial download size under 8 MB." (S1)
- The developer guide is slightly looser: "try to keep your game's file size as
  small as possible. Around 8 to 10MB is the size to aim for!" (S21)
- Loading is called "one of the most important topics for success on the web ...
  The amount of players who drop off during loading shouldn't be
  underestimated." (S21)
- Progressive loading is recommended: "only the most necessary files (main menu,
  tutorial level, first stage, etc.) are downloaded when the game starts. While
  the player is in this part of the game, the game downloads the remaining files
  in the background." (S21)
- Loading screens: "A loading bar is the perfect way to give the player a sense
  of progression while the game is getting ready. Especially visually engaging
  loading screen[s] that feature the logo or some visuals of your game will help
  prevent any early drop-offs by players mistaking the game as frozen or
  broken." (S21)
- The Inspector measures loading time and file size directly and warns on
  under-compressed images (S6).
- No explicit numeric load-time SLA is published. The proxy metric is
  conversion-to-play; the illustrative category average in Poki's own Web Fit
  example is **70 % C2P** (S9).

### 3.2 Frame rate, devices, reliability (contractual)

From the deal terms page (S13), the strongest numbers Poki publishes:

- "must be fully responsive and support 16:9 aspect ratio across devices and
  should run at a **minimum of 30 frames per second**, with a **target of 60
  FPS**, on the Supported Devices and Browsers under standard network conditions
  (e.g. **3G mobile data or better**)";
- supported devices = "**mid-range mobile phones released within the last three
  years**, running modern Android or iOS" and desktops on a browser version from
  the past 12–18 months;
- "must ... function without critical issues (such as crashes, major rendering
  errors, or input failures) for **at least 85 % of the users** on each
  Platform";
- "free of game-breaking bugs, including ... player progress resets, game
  freezes, infinite loading and/or major UI/UX malfunctions", fixed within one
  month of report.

Poki also ships a **Player Device Report** (`developers.poki.com/guide/player-device-report`,
login-gated) whose panels include WebGL version share, WebGPU support, aspect
ratios and device-pixel ratios — the right source before committing to a 3D
renderer feature set (S19 links it; panel names read from the site bundle).

### 3.3 Viewport, aspect ratio, mobile

- "Your game must scale up to cover the full canvas across all devices and have
  the aspect ratio of **16:9**. The correct dimensions to scale proportionally
  to are **640x360, 836x470, or 1031x580**." (S1, S2)
- "On mobile, your game should cover the entire screen in either the portrait or
  the landscape orientation. Or both if you aim for the best user experience."
  (S1)
- "Automatically force mobile control schemes on tablet devices." (S1) —
  `PokiSDK.getDeviceInfo().category` returns `mobile | tablet | desktop` and "is
  what the Poki platform itself uses" (S4).
- "Display appropriate control schemes (mobile controls on mobile/tablets,
  keyboard instructions on desktop)." (S1)
- "Prevent game viewport scrolling from affecting the parent page during review
  process" (S1); the snippet Poki gives cancels `ArrowUp/ArrowDown/Space`
  keydown defaults and `wheel` with `{ passive: false }` (S4).
- The **Poki Pill** overlays the game on mobile: 46×62 px below 1211 px width,
  92×64 px at/above; reposition with `movePill(topPercent, topPx)`, default
  `movePill(0, 24)`, cannot go below 50 % of the game area (S4). Keep that top
  strip free of critical HUD.
- **Fullscreen is not granted by default.** It is case-by-case; the usual
  criteria are multiplayer, open world, or hidden-object games. "If a game does
  not fit this criteria and plays well in the default frame, it will not be
  eligible." (S16) Design for the embedded frame, not fullscreen.

### 3.4 External requests, analytics, links

- "Poki blocks all external requests by default for games on our platform. This
  means your game may not call any third-party URLs unless they have been
  explicitly approved." (S12)
- Explicitly forbidden: Google Fonts, externally hosted images/audio, CDN code
  libraries (jsDelivr named), in-game chat systems (except under Poki Guardian),
  external account systems / social logins / anything collecting email or
  personal data (S12).
- Exceptions, case-by-case and requiring a live, in-game-linked Privacy Policy:
  externally hosted multiplayer servers; third-party analytics (GameAnalytics,
  ByteBrew, in-house). "Google related products (such as Google Analytics) are
  blocked in all instances." (S1, S12) Approved hosts are added to a custom CSP
  per game via the Settings page (S12).
- "Remove all splash screens and outgoing links from the game. Feel free to
  incorporate your studio's logo into the loading screen instead." (S1)
- "Since Poki's SDK comes out of the box with advertisements, no other ads are
  allowed in the game." (S1) "Only Poki's advertisement system is allowed."
- "Remove any in-game UI elements for purchasing currency or disabling
  advertisements." (S1)

### 3.5 Storage and saving

- "Your game must work in incognito mode. Google's Incognito Mode restricts
  access to localStorage, so you should wrap your localStorage operations in a
  try/catch block." (S1, S2)
- "Implement progress saving where appropriate, or clearly inform players when
  progress won't be saved upon exit." (S1)
- Cloud gamesaves are automatic for logged-in users: the SDK watches
  `localStorage` and IndexedDB and syncs them. Keys/stores prefixed
  `poki_ignore` are excluded. **Hard cap: 1 MB after gzip** — exceed it and cloud
  saves are silently disabled for that player. (S4)
- "Remove all development tools, debug code, and testing artifacts before
  publication." (S1) — including `PokiSDK.setDebug(true)` (S3).

### 3.6 Audio and autoplay

Poki's only stated audio rules are: mute during ads (S1, S4) and no externally
hosted audio files (S12). There is **no Poki rule requiring a user gesture
before audio**.

The constraint is the browser's, not Poki's (S25): audio autoplay needs user
interaction with the domain, an MEI threshold, or an installed PWA; a
cross-origin iframe needs `allow="autoplay"` delegation from the parent. Poki's
game iframe is served with
`allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share"`
(read from the P4D site bundle, S21), so autoplay permission *is* delegated —
but a `AudioContext` created before any gesture still starts `suspended` and
needs `resume()` on the first input. **[inference]** Start the audio context on
the first pointer/key event (the same event that fires `gameplayStart()`), and
never assume music will play on load.

---

## 4. Gameplay / UX guidelines that drive the rating

### 4.1 Entry and onboarding

- "Streamlined Entry: Minimize UI screens and menus — ideally place players
  directly into gameplay." (S1)
- "Skip the menu ... For first-time players, it's especially important to skip
  splash screens, title screens, and level selects. Let them jump right into the
  good part!" (S21)
- "Ensure all cutscenes and introductory sequences are skippable." (S1)
- "Design tutorials that are visual and intuitive rather than text-heavy." (S1)
  "A good tutorial explains to the player how the mechanics work in a way that
  feels natural, teaching them what the game is about without blocking the
  gameplay." (S21)
- Poki's cited pattern: in Subway Surfers "players can't die while they're moving
  through the onboarding. Instead, they get the chance to go back and try again
  until they get it right!" (S21)
- Localization is recommended for text-heavy games; suggested first wave is
  EFIGS, then CJK, then Brazilian Portuguese + Russian, with Turkish early
  (S1, and the localization guide read from the bundle).

### 4.2 Controls and accessibility

- "For keyboard-controlled games, implement ESC or spacebar for pause/resume
  functionality. Please make sure that the game fires the appropriate SDK events
  in that case." (S1)
- "Players should be able to navigate in the game using both mouse and keyboard
  controls. WASD or Arrow keys should navigate in the menu. Space bar or Return
  should activate the primary button." (S22)
- "Not all players can use standard keyboard inputs such as WASD. Where possible,
  we encourage supporting alternative control methods like mouse only input,
  drag steering, auto acceleration, or customizable keys." (S5)

### 4.3 Engagement

- Celebrate: "at the end of a level or whenever the player reaches a milestone
  ... some confetti, a nice sound effect and some words of praise." (S22)
- Clear goals: short-term plus long-term ("a new item, weapon or power-up to work
  towards"). (S22)
- Tune difficulty: "starting off easy and getting more difficult as you
  continue." (S22)
- Diagnosis rubric from Player Fit results (S8): most players in the first two
  playtime columns = onboarding/early-game problem; a bulge in the middle
  columns = "lacking content or a needed hook to keep players interested".

### 4.4 Save/restart

Only the two rules above (S1, §3.5): save progress where appropriate or tell the
player it will not persist; no progress resets (S13). Restart flow is governed
by the SDK order in §1.2 — every death→restart is a `gameplayStop()` →
`commercialBreak()` → `gameplayStart()` cycle, with no game-side ad timer.

---

## 5. Submission, testing and review flow

The funnel (S19, S7–S11):

| Level | Gate | Numbers |
| --- | --- | --- |
| 0 | Apply via the game submission form; P4D is closed beta with limited access. Curation looks at Quality (UX/feel + core loop), Player fit, Tech. | — |
| 1 | Upload build; meet Requirements (S1) and implement the SDK. | initial download < 8 MB; 16:9 |
| 2 | Playtesting — 10 recordings per test, with inputs, console output, duration, country, device. | must watch ≥ 5 recordings to unlock Level 3 |
| 3 | Player Fit Test — 500 players, no video, playtime only; ~5 h to run; 2 tests/day. | **pass = average playtime > 3 min AND ≥ 25 % of the 500 gameplays over 3 min** |
| 4 | Web Fit Test — ~10 000 players on a few category pages, 3–5 days, cannot be stopped once started; needs a static thumbnail. | scored 0–5 vs category averages on CTR, Average Time on Page, % Conversion to Play; 3 = at category average. Example averages Poki gives: 1.5 % CTR, 4 min ATP, 70 % C2P |
| 5 | Final Poki Review — 1–2 weeks; checks content against Quality Guidelines and "your game is unique enough among the category/categories your game falls into". | — |
| Release | Optional Technical Test (~5k gameplays/day, new-games category only), then Soft Release (scheduling 1–2 weeks, running 2–3 weeks, cannot be skipped), then Global Release (usually 2–3 weeks later, ~2-week discoverability push). | during soft release Poki expects "at least 1 Ad per DAU (with a nice split between Midroll and Rewarded videos)" |

Monetization events are **not** required for the Web Fit Test —
`commercialBreak()`/`rewardedBreak()` "can be saved for after the testing
phases", and placeholder ads run with no revenue (S9). `gameplayStart()` *is*
required by then (S9).

Thumbnails: static required before Web Fit; animated required before global
release; full-bleed square, at least 628×628 px, no borders/padding/letterbox,
avoid text ("Poki has run tests that indicate our players prefer text-free
thumbnails"), avoid colours near the playground background `#83FFE7` (S18).

Updates after release go through the same route: upload a build, request review,
Developer Support publishes (S11).

### 5.1 Common rejection reasons

From the Quality Guidelines (S5), verbatim categories:

- **Unsafe themes** — bullying, abuse, stereotypes, sexism, graphic violence,
  sustained scary themes, sex, inappropriate clothes, cheating, gambling,
  alcohol, tobacco. Adult themes and IP misuse are "rejected immediately".
- **Unlicensed IP or heavily inspired games** — "We do not work with, or feature
  direct copies or clones of original titles ... Even if a game isn't a direct
  copy, we may still not be able to work with your game if its visual style,
  themes, game modes, and overall concept resembles well-known existing IPs.
  Poki also does not work with games that reuse templates or assets with little
  modification or creativity." The published clone-vs-inspired axes are art
  style, core gameplay mechanics, level design / economy / progression,
  UI / icons / audio / font, characters / skins, and game name / thumbnail.
  Acceptable differentiation examples: "changing the theme entirely
  (post-apocalyptic, sci-fi, underwater, etc.)", "introducing a unique mechanic,
  twist, or polish — for example, adding time-loop mechanics, meta-game layers,
  or blending genres".
- **Low quality** — "buggy or broken games, excessive load size, poor
  performance, confusing UX or poor or inconsistent graphics".
- Fixable with significant improvement: similarity concerns, mildly
  inappropriate content, unpolished assets/confusing layout/uninspired theme.
  Poki also warns it may not explain a rejection in detail.

Oversaturation is an explicit rejection lever (S15): "If a genre or trend becomes
too crowded, we may reject additional similar submissions."

AI-assisted content policy (S14): AI is allowed but "must add value rather than
replace craft, quality, or originality". Do not ship raw AI art; no visible
watermarks or prompt text; maintain one art direction; do not use prompts that
smuggle in copyrighted styles; "assume AI art makes a clone original — if the
concept is copied, it's still a clone" is listed as a DON'T. "Games (solely)
created with the use of AI could create IP ownership issues for Poki. You should
therefore be able to demonstrate how and which tools were used, and be able to
provide Poki with an overview of the game creation process and prompts and
iterations upon request." AI-generated or procedurally generated levels must be
manually tested, tweaked and balanced.

---

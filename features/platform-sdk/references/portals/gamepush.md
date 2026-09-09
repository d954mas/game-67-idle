---
type: Reference Lesson
title: GamePush integration (research packet)
description: GamePush's publisher model, its mirrored SDK loader, ad formats, player-field cloud saves and the multi-host constraint it puts on a single build, mapped onto this repo's platform-sdk wrapper contract.
tags: [game-knowledge, gamepush, web-release, sdk]
status: research
sources_checked: 2026-09-09
---

# GamePush integration — primary-source packet

Every claim below comes from GamePush's own documentation and typed SDK
reference. Where the documentation is silent this packet says **[docs silent]**
instead of guessing.

Source index:

- S1 `https://docs.gamepush.com/docs/get-start/` — the documentation map
- S2 `https://docs.gamepush.com/docs/get-start/getting-started/` — the install snippet and `onGPInit`
- S3 `https://docs.gamepush.com/docs/advertising/` — the four ad formats, their flags and events
- S4 `https://docs.gamepush.com/docs/player/` — the player model and its fields
- S5 `https://docs.gamepush.com/docs/player/cloud-saves/` — how progress is stored and its size limits
- S6 `https://docs.gamepush.com/docs/player/player-manager/` — `ready`, `sync`, `login`, storage targets
- S7 `https://docs.gamepush.com/docs/storage/` — the key/value storage module
- S8 `https://docs.gamepush.com/docs/platforms/` — runtime platform detection
- S9 `https://gamepush.com/sdk/docs/classes/GamePush.html` — the root object's typed surface
- S10 `https://gamepush.com/sdk/docs/classes/Ads.html` — ad method signatures and return types
- S11 `https://gamepush.com/sdk/docs/classes/MyPlayer.html` — the player object's typed surface
- S12 `https://docs.gamepush.com/tutorials/publishing-the-game-on-platforms-guide/` — hosting, distribution, per-platform requirements
- S13 `https://gamepush.com/service/distribution-agreement/` — the revenue split by developer status
- S14 `https://docs.gamepush.com/docs/leaderboards/leaderboard/` — the global board, its query and its result
- S15 `https://gamepush.com/sdk/docs/classes/Leaderboard.html` — the board's typed surface, global and scoped

Not read for this packet, and therefore not described here: purchases,
multiplayer, channels, achievements, A/B experiments, rewards, segments,
triggers, and the isolated (scoped) leaderboards.

Repo files this packet maps onto: `features/platform-sdk/references/contract.md`,
`features/platform-sdk/web/adapters/gamepush.js`,
`features/platform-sdk/publish-targets/gamepush.json`.

---

## 1. What GamePush is

GamePush is not a portal. It is a publisher SDK plus hosting: one archive is
uploaded to its Game Hosting and GamePush re-serves that same build to the
platforms selected in the panel — S12 lists Yandex.Games, VK, OK, Mail.ru,
CrazyGames, Telegram, Y8, Kongregate, Poki, GameDistribution, GameMonetize,
CoolMath, Playdia, Playgama, VK Play, SmartMarket and others, "24+" in total.

The consequence that shapes the adapter: **the build cannot know which host it
woke up on.** Anything host-specific has to be asked of the SDK at runtime
(S8: `gp.platform.type`, `gp.platform.isExternalLinksAllowed`,
`gp.platform.isSupportsCloudSaves`, `gp.platform.isBackendAllowed`), never
decided at build time. This repo's capability flags are build-time, so the
`gamepush` descriptor takes the intersection: no external links.

The revenue split depends on the developer's legal status (S13, §5.1): 40% for
a private individual, 30% for a self-employed person, sole trader or company,
20% on partner platforms. Payout follows within 90 working days (§5.2).

## 2. Loading the SDK

S2 gives the install snippet as an obfuscated loader. Decoded, it holds four
mirrors of one file:

```text
https://gs.eponesh.com/sdk/game-score.js
https://s3.gamepush.com/files/gs/sdk/game-score.js
https://s3-eu.gamepush.com/sdk/game-score.js
https://gamepush.com/sdk/game-score.js
```

They are probed and the first that answers is used; the snippet also gives up
after 5000 ms. The query string carries the project's identity and the name of
the global callback:

```text
?projectId=<id>&publicToken=<token>&callback=onGPInit
```

The SDK does not resolve a promise — it calls that global with the instance:

```javascript
window.onGPInit = async (gp) => {
    await gp.player.ready;
    await gp.ads.showPreloader();
    gp.ads.showSticky();
};
```

Both parameters are client-side values, present in the page source of every
published build; `publicToken` is public by construction. **[docs silent]** on
whether a mirror that answers 200 with an error body is distinguishable from a
healthy one, so the adapter treats only a script error as a reason to fall
through to the next mirror.

## 3. Readiness and the game lifecycle

S9 gives the root object:

```text
ready: Promise<GamePush>      gameStart(): Promise<void>
isGameStarted: boolean        gameplayStart(): Promise<void>
isGameplay: boolean           gameplayStop(): Promise<void>
isPaused: boolean             pause(): void / resume(): void
language: Lang                locale: string
platform: Platform            isMobile / isPortrait / device
```

`gameStart()` is the single readiness milestone: there is no progress channel,
so GamePush draws its own loading screen and learns only that the game is up.
`gameplayStart()` / `gameplayStop()` bracket the interval in which the
publisher's own ad breaks may fall.

## 4. Ads

S3 and S10 agree on the surface. Every show method returns `Promise<boolean>`
(`closeSticky` returns `Promise<void>`):

```javascript
gp.ads.showFullscreen({ showCountdownOverlay: true });
gp.ads.showRewardedVideo({ showFailedOverlay: true });
gp.ads.showPreloader();
gp.ads.showSticky();  gp.ads.refreshSticky();  gp.ads.closeSticky();
```

Availability and state are plain flags, not calls: `isFullscreenAvailable`,
`isRewardedAvailable`, `isStickyAvailable`, `isPreloaderAvailable`, the matching
`is*Playing`, plus `isAdblockEnabled`, `canShowFullscreenBeforeGamePlay`,
`needToShowResumeOverlay` and `needToLeaveFullscreenBeforeAds`.

Events fire for the generic pair and per format:

```javascript
gp.ads.on('start', () => {});          gp.ads.on('close', (success) => {});
gp.ads.on('fullscreen:start' | 'fullscreen:close' | 'rewarded:start'
        | 'rewarded:close' | 'rewarded:reward' | 'preloader:start'
        | 'preloader:close' | 'sticky:start' | 'sticky:render'
        | 'sticky:refresh' | 'sticky:close', handler);
```

The preloader is the loading-screen ad and is legal only before `gameStart()`.
The sticky banner shares the page with a running game, which is why only the
three full-window formats may pause it. S12 notes the sticky banner is worth
"up to 70% of the revenue from your entire advertising monetization" on
Yandex.Games.

## 5. Cloud saves

S5 is explicit: the SDK requests the player's profile from the GamePush server
at boot (platform auth, or a guest secret code, creating a profile if none
exists), the game changes fields, and **nothing is stored until `sync()`**:

```javascript
const level = gp.player.get('level');
gp.player.set('level', level + 1);
await gp.player.sync();
```

Fields are declared in the project panel, one per stored value, typed number,
string, boolean or JSON; names take Latin letters, digits, `_` and `-` (S4).
A field the panel does not declare is not an error — the value is simply not
part of the profile, which is why the adapter reads the value back after
`set()` before it reports a write.

Size limits (S5): total profile data must stay **under 1 MB**, with **under
10 KB gzip recommended**; a sync averages 7 ms.

`gp.player.ready` (S6, S11) resolves when the first load is done. Reading a save
before it resolves would see an empty profile, so the adapter awaits it as part
of SDK init. `sync(opts)` can target `preferred`, `cloud`, `platform` or
`local`; the default follows the panel.

S7's `gp.storage` is a different thing: a key/value module whose platform mode
exists only where the host has a store (it names Yandex Games and CrazyGames),
falling back to LocalStorage / IndexedDB. It is therefore not a cross-platform
cloud save, and this game does not use it.

## 6. Identity and login

S11 gives `isLoggedIn`, `isLoggedInByPlatform`, `hasAnyCredentials`, `id`,
`name`, `avatar`, `isStub`, and `login()` returning `Promise<boolean>`; the
overlay resolves when it closes, whether or not the player signed in, so the
flag afterwards is the outcome.

## 7. Leaderboards

Two kinds, and the panel names them apart. The **global** board is assembled by
the publisher out of player fields -- it has no id, no console entry, and
nothing to create: whatever the player's `score` field holds is their standing.
The **isolated** boards (S9's `fetchScoped`/`openScoped`, created in the panel)
are the daily, per-level and tournament ones, addressed by tag and variant.

This game takes the global board, which makes a score a field write:

```javascript
gp.player.set('score', value);
await gp.player.sync();
```

Reading and showing it (S9, and the query shape from the leaderboard docs):

```javascript
gp.leaderboard.fetch({ orderBy: ['score'], order: 'DESC', limit: 20,
                       includeFields: ['score'], withMe: 'first', showNearest: 5 });
gp.leaderboard.open();
```

`fetch` answers with `{ players, fields, topPlayers, abovePlayers, belowPlayers,
player }`, and a row carries `id`, `name`, `avatar`, `position` and the ranked
fields. `open()` draws the publisher's own overlay over the canvas.

Consequence for this repo: `gamepush` is a portal board family in
`features/leaderboard`, like Yandex, and the board id the manifest carries is a
formality -- the global board is one board, so the adapter ignores it. The day
scope stays with the self-hosted targets: only an isolated board could reset,
and this game does not create one.

## 8. What the platform requires of the build

From S12, the requirements that reach the artifact:

- One archive, `index.html` at its root, Latin file names.
- The install parameters belong to the project; a build carrying another
  project's `projectId` reports to that project.
- Some distribution targets add their own rules — CoolMath and Playdia forbid
  external links and online services (leaderboards, chats, profiles) and require
  `gp.analytics.goal('LEVEL_START', n)` events. Those targets are opt-in per
  game in the panel, and this game does not select them.
- Yandex.Games, VK, OK and Mail.ru need their own app ids and secret keys
  entered in the GamePush panel; the build carries none of them.

**[docs silent]** on a maximum archive size for GamePush hosting.

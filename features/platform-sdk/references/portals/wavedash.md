---
type: Reference Lesson
title: Wavedash integration (research packet)
description: Wavedash publishing model, injected SDK contract, cloud-save and leaderboard semantics read from the shipped SDK build, mapped onto this repo's platform-sdk wrapper contract.
tags: [game-knowledge, wavedash, web-release, sdk]
status: research
sources_checked: 2026-09-08
---

# Wavedash integration — primary-source packet

Every claim below comes from Wavedash's own developer documentation
(`docs.wavedash.com`) or from the published SDK package, read directly at
`@wvdsh/sdk-js@1.3.48` (`dist/index.d.ts` and `dist/index.js`). Where the
documentation is silent this packet says **[docs silent]** instead of guessing.

Source index:

- S1 `https://docs.wavedash.com/` — docs root and section map
- S2 `https://docs.wavedash.com/getting-started/quickstart` — build folder, `wavedash.toml`, `wavedash dev`, upload
- S3 `https://docs.wavedash.com/sdk/setup` — injection, `init()`, ready-state getters, `deferEvents`
- S4 `https://docs.wavedash.com/sdk/functions` — the full function surface
- S5 `https://docs.wavedash.com/sdk/events` — event names and payloads
- S6 `https://docs.wavedash.com/sdk/players` — identity, JWT, friends, avatars, presence
- S7 `https://docs.wavedash.com/sdk/leaderboards` — boards, sort/display, metadata limits
- S8 `https://docs.wavedash.com/sdk/cloud-saves` — remote files, paths, metadata
- S9 `https://docs.wavedash.com/sdk/audio` — mute mirror and its precedence rule
- S10 `https://docs.wavedash.com/publishing/monetization` — paid content, creator fund, 10% fee
- S11 `https://docs.wavedash.com/publishing/upload` — immutable numbered builds, 1 GB limit
- S12 `https://docs.wavedash.com/cli` — `wavedash auth login`, `dev`, `build push`, `publish`
- S13 `@wvdsh/sdk-js@1.3.48` — the shipped SDK build and its type declarations

Repo files this packet maps onto: `features/platform-sdk/references/contract.md`,
`features/platform-sdk/web/adapters/wavedash.js`,
`features/platform-sdk/publish-targets/wavedash.json`.

---

## 1. What the platform is

Wavedash distributes browser games and runs them in a frame inside its own host
page (S1). The player signs in to Wavedash before launching a game, so a game
never sees an anonymous session (S6). The platform sells **paid content**: a
one-time in-game unlock, with a 10% marketplace fee, plus a creator fund (S10).

There is **no advertising inventory of any kind** — no interstitial, no
rewarded, no banner. The word does not appear in the monetization page, the
function reference, or the SDK build (S4, S10, S13). This is the single largest
difference from every other portal this wrapper supports, and it is why the
`wavedash` descriptor declares `ads_supported: false` and
`rewarded_supported: false`.

## 2. SDK loading

There is no script tag and no CDN URL. The host page defines the `Wavedash`
global inside the game frame before the game's own code runs (S3). The npm
package exists only to supply TypeScript types for a game that wants them (S2);
its runtime is the same object the host injects.

`wavedash dev` runs the build inside a local sandbox that provides the same
global (S2, S12), which is the only way to exercise the adapter off-platform.

**Consequence for this wrapper:** the adapter loads nothing. It waits a bounded
moment for the global, then reports readiness. `sdk_policy.sdk_url` in the
target manifest is intentionally empty, and there is no URL that must survive
minification.

## 3. Loading screen and reveal

The host draws a loading screen over the frame, and the game is hidden until it
says it has loaded (S3). Three functions matter (S4, S13):

- `updateLoadProgressZeroToOne(progress)` — reports a `0..1` fraction to the host;
- `loadComplete()` — declares loading finished and starts the session heartbeat;
- `init(config?)` — synchronous, returns a boolean, **calls `loadComplete()`
  internally** and then applies the config.

So `init()` is the reveal. Calling it at boot would lift the host loading screen
over an empty canvas. The adapter therefore calls `init()` at the first loading
milestone the game reports as finished, and forwards progress until then.

`init()` also calls `readyForEvents()` unless `deferEvents: true` was passed, so
the default config needs no second call (S13).

If none of the three is called within 10 seconds the SDK logs a console warning
(S13). That warning is the fastest signal that the wrapper's loading milestones
never reached the adapter.

## 4. Identity

`getUserId()`, `getUsername()` and `getUserAvatarUrl(userId, size)` are
synchronous and answer for the signed-in player, whose record the SDK caches at
setup (S6, S13). `AvatarSize` offers 64/128/256 px, and any pixel size is
accepted.

There is no login call, because there is nothing to open: the player arrived
signed in. The adapter therefore publishes the player through `getPlayer()` and
answers `login()` with `unsupported`, the same shape every non-Yandex adapter
uses.

`getUserJwt()` returns a short-lived (1 hour) signed token for a game's own
backend (S6). This game has none; the call is unused here.

## 5. Cloud saves are files

Wavedash has no key/value store. It has a per-player remote file root and a
local file layer, and the SDK moves bytes between them (S8, S13):

- `writeLocalFile(path, Uint8Array)` / `readLocalFile(path)` — the local layer is
  **IndexedDB**, and the type declarations mark it as the path for pure-JS
  games; engine builds are told to use their own FS instead;
- `uploadRemoteFile(path)` / `downloadRemoteFile(path)` — move that file to and
  from the player's remote root;
- `remoteFileExists(path)` — a HEAD request that returns `false` for a missing
  file and throws only on a real error (S13);
- `listRemoteDirectory`, `downloadRemoteDirectory`, `deleteRemoteFile`.

Remote keys use forward slashes and are relative to the player root (S8). Async
calls answer `{ success: true, data }` or `{ success: false, data: null, message }`.

**Mapping onto the wrapper's storage outcomes.** A key becomes
`saves/<key>.json`. A read is `remoteFileExists` → `missing` when it answers
false, then `downloadRemoteFile` + `readLocalFile`; a write is `writeLocalFile`
then `uploadRemoteFile`, and only a successful upload is `acknowledged`. The
distinction the wrapper needs — a missing key is not a failure — is available
because `remoteFileExists` refuses to throw for absence.

**[docs silent]** on a per-file or per-player size limit, and on whether an
acknowledged upload is durable beyond the object store's own write. The upload
page's 1 GB figure is the build, not the save (S11).

## 6. Leaderboards

A board is addressed by a backend id, not by its name, so a name is resolved
once through `getOrCreateLeaderboard(name, sortOrder, displayType)`, which
creates the board when the console has none (S7, S13). `LeaderboardSortOrder` is
`ASC`/`DESC`; `LeaderboardDisplayType` is `NUMERIC`, `TIME_SECONDS`,
`TIME_MILLISECONDS`, `TIME_GAME_TICKS`.

- `uploadLeaderboardScore(id, score, keepBest, ugcId?, metadata?)`;
- `listLeaderboardEntries(id, offset, limit, friendsOnly?)`;
- `listLeaderboardEntriesAroundUser(id, countAhead, countBehind, friendsOnly?)`;
- `getMyLeaderboardEntries(id)` — the player's own row;
- `getLeaderboardEntryCount(id)` — synchronous, served from cache.

Entries carry the player's `userId`, `username` and `userAvatarUrl`, the score,
the rank, and the metadata that was submitted with it (S7, S13). Metadata is at
most 16 pairs, keys ≤ 64 chars, string values ≤ 256 chars, 2048 bytes total,
values limited to strings, numbers and booleans; breaking a limit fails the
upload and **does not save the score** (S7). The wrapper's `extra` payload
travels as one such string value.

Both reads and writes work without any login step, so the adapter answers
`canRead` and `canWrite` true and `needsLogin` false. The host draws no board of
its own and offers no call to open one, so `nativePopup` is false and
`showLeaderboard` is `unsupported`.

Note the docs and the shipped signature disagree on the fourth argument of
`listLeaderboardEntries`: the docs call it `includeMetadata`, the type
declarations call it `friendsOnly` (S7, S13). The build is authoritative.

## 7. Host-owned UI: mute, fullscreen, links

The host page owns the mute switch and mirrors it into the frame. `isMuted()`
reads the mirror, `requestMute(muted)` asks, and `MUTE_CHANGED` broadcasts the
result. **A game cannot unmute over a mute the player set in the Wavedash UI**
(S9). The adapter follows this state into the wrapper's audio lifecycle and
never drives it.

Fullscreen is the same shape — the host owns the target, `FULLSCREEN_CHANGED`
reports it, and entering needs a user gesture (S13).

`copyLink(url)` is how a game offers an external URL: the link is copied to the
player's clipboard and the host shows a toast, rather than navigating the player
out of the game (S13). There is no navigation seam, which is why the descriptor
declares `external_links_allowed: false`.

**[docs silent]** on pause and resume. There is no lifecycle event for the host
suspending the game, and none of the 19 documented events is a pause (S5), so
the adapter bridges no portal pause.

## 8. Events

`Wavedash.on(name, handler)` returns an unsubscribe function; `off` also exists
(S5). The names live on `Wavedash.Events`. Nineteen events cover lobbies, P2P,
backend connectivity, stats storage, fullscreen, mute and entitlements. Only
`MUTE_CHANGED` matters to this wrapper.

## 9. What this wrapper does not use

Multiplayer lobbies and WebRTC P2P, user-generated content, achievements and
stats, friends and presence, and paid content are all real SDK surfaces (S4)
with no seam in the C facade. They are listed here so a later game does not
conclude the platform lacks them.

## 10. Publishing

A build is a folder of static files with `index.html` at its root (S2). Project
config is `wavedash.toml` with `game_id` and `upload_dir`. The CLI does
`wavedash auth login`, `wavedash dev`, `wavedash build push`, and
`wavedash publish`; the developer portal accepts a drag-and-dropped folder or
zip instead (S2, S12). Each upload is an immutable numbered build and older
builds stay available for rollback, up to 1 GB (S11).

Content rules cover cryptocurrency, gambling, discrimination and impersonation,
plus performance and cover-art standards. **[docs silent]** on third-party
analytics, outbound network calls, and self-hosted SDKs.

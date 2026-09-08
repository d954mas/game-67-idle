---
type: Reference Lesson
title: Pikabu Games integration (research packet)
description: Pikabu Games publishing model, the portal-hosted SDK, its ad formats, signed player identity, and the two gaps it leaves to the game's own backend, mapped onto this repo's platform-sdk wrapper contract.
tags: [game-knowledge, pikabu, web-release, sdk]
status: research
sources_checked: 2026-09-08
---

# Pikabu Games integration — primary-source packet

Every claim below comes from Pikabu's own SDK documentation, read directly at
`games.pikabu.ru/sdk/docs`. Where the documentation is silent this packet says
**[docs silent]** instead of guessing.

Source index:

- S1 `https://games.pikabu.ru/sdk/docs/intro/quick-start` — the six publishing steps
- S2 `https://games.pikabu.ru/sdk/docs/intro/what-do` — what the SDK offers
- S3 `https://games.pikabu.ru/sdk/docs/sdk/init` — script tag, `PkbSDK.init()`, `gameStarted()`
- S4 `https://games.pikabu.ru/sdk/docs/sdk/player` — identity, id lifetime, auth dialog
- S5 `https://games.pikabu.ru/sdk/docs/sdk/ads` — the three formats and their errors
- S6 `https://games.pikabu.ru/sdk/docs/sdk/events` — the event surface
- S7 `https://games.pikabu.ru/sdk/docs/sdk/verification` — signed data, JWT shape, secret key
- S8 `https://games.pikabu.ru/sdk/docs/sdk/testing` — the Studio test environment
- S9 `https://games.pikabu.ru/sdk/docs/requirements/tech` — technical requirements and checklist
- S10 `https://games.pikabu.ru/sdk/docs/requirements/content` — content rules
- S11 `https://games.pikabu.ru/sdk/docs/requirements/ads` — where ads may run
- S12 `https://games.pikabu.ru/sdk/docs/studio/adding-game` — the Studio game card
- S13 `https://games.pikabu.ru/studio` — the developer cabinet itself

Not read for this packet, and therefore not described here: the in-app purchase
page, featuring, age rating, moderation, and statistics.

Repo files this packet maps onto: `features/platform-sdk/references/contract.md`,
`features/platform-sdk/web/adapters/pikabu.js`,
`features/platform-sdk/publish-targets/pikabu.json`.

---

## 1. What the platform is

Pikabu Games is the game showcase of Pikabu, a large Russian user-generated
content site (S2, S13). The SDK and the Studio are in open beta, which the
cabinet states on every page (S13).

The publishing path is six steps: meet the requirements, integrate the SDK,
register in the Studio, generate a secret key, test and submit for moderation,
get paid (S1).

**The portal hosts nothing.** "На данный момент мы не поддерживаем загрузку
билда игры на серверы Пикабу" — the build lives on the developer's own server
and the Studio stores only its URL (S9, S12). The documentation names GamePush
as the alternative for a developer unwilling to host (S9); that route is a
third-party aggregator and is not what this adapter implements.

## 2. SDK loading

One script tag from the portal's own origin (S3):

```html
<script async src="https://games.pikabu.ru/sdk/sdk.js" onload="initSDK()"></script>
```

The global is `PkbSDK`, and `PkbSDK.init()` is documented as a once-per-game
call returning the instance every later call uses (S3). The SDK "работает
только внутри страницы игры на платформе Пикабу Игры или в тестовом окружении
Студии" — outside those two contexts it does not work at all (S3).

**Consequence for this wrapper:** the URL cannot be self-hosted or mirrored, so
it must survive minification and is declared in `required_markers`. There is no
offline exercise of this adapter: the Studio test link is the only way to run it
(S8). A blocked or missing script leaves the adapter operational as no-ops,
because the portal requires an adblocked player to still play (S9).

## 3. Readiness and the loading screen

`sdk.gameStarted()` tells the platform the game is loaded and interactive, and
is called after the main resources are loaded and the first screen is prepared
(S3). There is **no progress channel** — nothing takes a 0..1 fraction — so the
wrapper's loading milestones collapse into this single call.

The one thing that has to happen before it is the preloader ad (S5, below).

## 4. Ads: three formats, one shape

Every format carries `isSupported: boolean`, `canShow(): Promise<boolean>` and
`show()`, and `canShow()` must be called immediately before each show (S5).
`show()` resolves `{ rendered: true }` or `{ rendered: false, reason: AdError }`,
with the rewarded result carrying an extra `reward: boolean` (S5).

- **preloader** — shown while the game loads, no close timer, **mobile only and
  only before `gameStarted()`**; calling it afterwards fails with
  `INVALID_GAME_STATE`. Loading may continue behind it, but music must be paused
  (S5).
- **fullscreen** — 3-second timer, and repeat shows are refused inside 120
  seconds (S5). Ads belong between levels, after a run/match/round ends, or on
  entering menus, shops and result screens; never during active play or over
  interactive UI (S11).
- **rewarded** — 10-second timer, no frequency limit, and reward is granted only
  when `result.reward` is true. It must be started by an explicit player action,
  and its button must be hidden when `canShow()` said no (S5, S11).

`AdError` is `NOT_SUPPORTED | IN_PROGRESS | UI_BUSY | COOLDOWN_ACTIVE |
INVALID_GAME_STATE | UNKNOWN` (S5). Unavailability is a normal outcome: it must
not surface as a technical error, must not leave the game paused, and must not
block progression (S5, S11).

Ads are not required for publication — a game that never calls them is fine (S5).

## 5. Identity

`sdk.player` carries `id`, `name`, `avatar` and `isAuthorized`, available right
after init (S4). The id is per-game and exists for an anonymous player too: it
is kept in the browser and reused on later runs, "чтобы сохранять прогресс
игрока до авторизации" (S4).

Two consequences the wrapper must respect (S4):

- **The id can change inside a session.** Signing in keeps the anonymous id only
  when that account has no earlier sessions in this game; otherwise the id
  becomes the account's. The docs say plainly to use the current value after
  authorization.
- **There is no logout.** The only transition is anonymous → authorized, which
  matches the facade's own rule.

`sdk.auth.openAuthDialog()` opens the login dialog and resolves when it closes,
whether or not the player signed in, so `isAuthorized` is what decides the
outcome (S4). Authorization can also be started by the player through Pikabu's
own interface, outside the game (S4).

## 6. Events

`on`, `once` and `off`, with `on` returning an unsubscribe function. One event
is documented: `userAuthorized`, carrying `{ id, name, avatar }` (S6).

**[docs silent]** on pause, resume, mute and visibility events. The pause and
audio duties in the technical requirements (S9) are therefore the game's own
work against the browser, not a portal signal to follow.

## 7. Saves: the platform stores nothing, and requires them anyway

Cloud saves are a publication requirement — "Облачные сохранения (обязательны
для публикации)" (S9) — while the SDK exposes no key/value store and no file
API (S2, S4). These are consistent, not contradictory: the build already runs on
the developer's server, so the save backend is the developer's too, and the SDK
supplies the identity to key it by. The docs state it directly: "sdk.player.id
может использоваться как ключ для хранения прогресса игрока на backend игры"
(S4).

`player.getSignedData()` is what makes that trustworthy (S7). It returns a JWT
signed HS256 with the game's secret key from the Studio; the payload for a
player is `{ avatar, id, isAuthorized, name }`, camelCase and alphabetically
sorted. The backend verifies the signature and only then trusts the data; the
key must never reach client code, and rotating it in the Studio invalidates
every signature issued before the change (S7).

Because `isAuthorized` is a payload field rather than a precondition, **an
anonymous player is signed too** — a save exists and is attributable before any
login.

**[docs silent]** on the token carrying `exp` or `iat`: the documented payload
has neither, so a captured token stays usable. For a save-only backend the loss
is another player's save document, and this game sells nothing.

## 8. Leaderboards

The SDK documents none (S2, S5, S6). A game that wants a board brings its own
backend, exactly as it does for saves; the adapter answers every board call
`unsupported`.

## 9. Testing

The Studio creates a test environment from the game's own URL and returns a link
that runs the build inside the platform frame (S8). Ads render as placeholders
and purchases use a mock payment sheet, while authorization behaves as in
production. The link works without authorization, which is why the docs
recommend deleting it once integration is debugged. An unauthorized player is
tested through an incognito window (S8).

This is the only environment where the adapter can be exercised at all (S3).

## 10. What the portal demands of the game

From the technical checklist (S9):

- the build is on the developer's own server;
- latest Chrome, Yandex Browser and Safari, desktop and mobile;
- **a Russian-language interface by default** — the player sees Russian at
  launch;
- correct scaling, nothing off-screen or overlapping;
- **auto-pause** on tab switch, ad, auth dialog and payment form;
- **sound off in the background** when the tab is switched or the browser
  minimized;
- playable with an adblocker enabled;
- cloud saves;
- no registration or authorization in third-party services.

From the content rules (S10): at least 30 minutes of total gameplay; titles,
descriptions, icons and screenshots must match the actual game and use Russian
or registered trademarks rather than foreign words; no sexual content, gambling
mechanics, political or religious agitation, hate, or copyright infringement.
The platform may refuse publication against its internal standards.

## 11. The Studio game card

Required (S12): title ≤ 50 characters; the game URL; supported devices; screen
orientation; age rating; one primary genre; short description ≤ 250 characters;
a square icon ≥ 1024×1024 and a horizontal icon ≥ 1920×1080 (16:9), JPG/PNG/WebP
up to 10 MB each. Optional: long description ≤ 10 000 characters, controls,
SEO description ≤ 250 characters, screenshots ≥ 1280×720 in 16:9 or 9:16.

The secret key is generated on the same card and must be copied when created
(S12).

## 12. Money

**[docs silent]** in the pages read here on the revenue split; the quick start
says only that payouts follow moderation (S1). The offer document linked from
the Studio help page is where those terms live.

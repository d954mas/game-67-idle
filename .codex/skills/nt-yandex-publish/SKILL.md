---
name: nt-yandex-publish
description: "Use when preparing a game in this repository for publication on Yandex Games: building the yandex target, verifying the SDK against the portal's own dev proxy, walking the moderation checklist, and producing the store asset set (icon, cover, screenshots, texts) for the console draft. Triggers include requests to publish, submit, upload, or prepare assets for Яндекс Игры / Yandex Games, and any moderation-rejection follow-up."
---

# NT Yandex Publish

Moderation on Yandex Games runs up to a month, and a rejection costs the whole
month. Everything here exists so that no requirement is answered from memory.

The portal's own list is the authority:
[requirements](https://yandex.com/dev/games/doc/en/concepts/requirements),
[local launch](https://yandex.com/dev/games/doc/en/concepts/local-launch),
[game events](https://yandex.com/dev/games/doc/en/sdk/sdk-game-events).

## Order Of Work

Build and SDK first, store assets second. A beautiful draft attached to a build
that never calls `LoadingAPI.ready()` is rejected on a technical requirement,
and the month starts again.

### 1. Build the target

```
node tools/game.mjs test
node tools/game.mjs package --target yandex
unzip -l release/artifacts/<game>-yandex-<hash>.zip
```

`package` runs the asset audit and a browser smoke over the packaged archive.
The listing answers two requirements on its own: `index.html` at the root with
ASCII names (1.23), and everything under 100 MB uncompressed (1.22).

### 2. Prove the SDK against the portal's own proxy

The portal ships a dev server, so "cannot be tested locally" is never true:

```
npx --yes @yandex-games/sdk-dev-proxy -p build/wasm-release-yandex/bin --dev-mode=true --port 8099
node tools/yandex_sdk_probe.mjs https://localhost:8099
```

`--dev-mode=true` needs no draft and mocks the SDK; the probe drives headless
Chrome and prints what the SDK actually saw. What must appear:

- `LoadingAPI.ready()` in the SDK's own console group — requirement 1.20;
- `GameplayAPI.start()` after the first press, `stop()` when the game pauses;
- `__portalPaused` flipping on `game_api_pause` / `game_api_resume`;
- a locale from `environment.i18n.lang`, and the game adopting it (2.10);
- no exceptions.

With a draft id, the same proxy runs against the real platform (`--app-id=<id>`,
no `--dev-mode`), and the debug panel opens with `&debug-mode=16` on the game
URL. That pass is the lead's: it needs the console account.

### 3. Walk the checklist and write it down

Every game keeps its own verdicts in
`games/<id>/design/knowledge/yandex_release_checklist.md`: one row per
requirement, a verdict, and what proves it. A row without evidence is a row
nobody checked.

The rows that catch games out, in order of how often they do:

- **1.3** sound stops when the game loses focus. Focus and visibility are
  different questions: the mixer answers focus, the clock answers visibility.
- **1.20** `LoadingAPI.ready`, `GameplayAPI.start/stop`, and the
  `game_api_pause` / `game_api_resume` events all handled.
- **4.7** during a fullscreen or rewarded ad both the sound and the gameplay
  are paused.
- **4.4** ads only at logical pauses, never inside a fight.
- **2.10 / 2.14** the language comes from the SDK. The check is not "the game
  ends up in the right language": the console watches for a READ of
  `ysdk.environment.i18n.lang` **while the game loads**, and the debug panel's
  i18n light is the verdict. A game that asks for the locale once, late, or
  only on a save-less run reads its answer from `navigator.language` and is
  refused with "автоматическое определение языка не реализовано" while running
  in perfect Russian. Read the locale where the SDK resolves, cache it, and let
  the game adopt it on every launch until the player picks one by hand.
- **1.11 / 1.13** progress is saved immediately and survives a refresh, and if
  the draft declares cloud saves it must actually cross devices. The portal's
  player storage is asynchronous and a game loop is not, so the read starts as
  early as the SDK exists and is awaited at the barrier the pack download
  already occupies. Which copy wins is a stamp comparison with one exception
  that decides the feature: a browser holding no save takes the account's copy
  regardless of stamps, because a new game stamps itself with the current time
  and would beat the run the player left elsewhere.
- **1.6/1.7** no system context menu on right click or long press, no page
  scrolling, no text selection.
- **1.9** no absolute URLs to Yandex S3 in the code.
- **5.2** the name is the same everywhere the player can read it — the store
  listing, the game, and the loading screen a shell template is happy to ship
  with a placeholder.

### 4. Produce the store asset set

Everything goes to `games/<id>/release/store/yandex/` and is validated
mechanically:

```
node ai_studio/store_kit/check.mjs --portal yandex --dir games/<id>/release/store/yandex
```

The spec it checks against is `ai_studio/store_kit/yandex_spec.json`. **The draft
form in the console is the authority on sizes**; when it disagrees with the
spec, the form wins and the spec file is updated in the same commit.

**Screenshots** come from the game itself, never from a mockup: run the game's
own capture bot (`devapi/*_bot.py`, `nt-runtime-automation`) at the declared
resolutions and pick beats that are gameplay — the portal wants gameplay across
at least 70% of the frame (5.1). Frames must carry no debug overlay, no FPS
counter, no system status bar and no portal interface (8.7).

**Every material is filmed once per language.** The draft keeps its own set of
screenshots and videos for each declared language and mirrors the Russian one
into the others until the checkbox beside the field is cleared, so the default
outcome is a Russian card showing an English HUD. Which language the game runs
in is decided at startup, when the string table is chosen — a setting flipped
mid-run does not change it — so the capture bot writes the language into a save
and relaunches (`--lang ru|en`, the shape `promo_video_bot.py` uses). The
screenshot set splits again by device: desktop is landscape, mobile is
portrait, and the form shows one set at a time behind a select. Five of each per
language is twenty files for two languages, and the checker counts them.

**Icon and cover may NOT be screenshots** (5.6). They come from the game's own
art through the asset pipeline (`nt-asset-workflow`, source-first): a square
icon with no rounded corners and no border (8.6), reading at 64 px in a
catalogue grid; a cover in the form's own ratio. Both are game assets and carry
license, provenance and hashes like every other asset in the repository.

**The horizontal gameplay video is required** and the draft cannot be sent
without it: 16:9, MP4, height from 400 px, up to 100 MB, up to 28 seconds. It
is filmed from the game like the screenshots — a bot plays a real run and every
second simulated frame is captured, then ffmpeg encodes the sequence at 30 fps
(`devapi/promo_video_bot.py` is the working shape). Watch
for the native build's own overlays: a mock interstitial from a commercial
break will sit in the middle of the shot, and the frame it lands on is not in
`ui.tree`, so the bot cannot dismiss what it cannot see — pick a stretch of the
run that asks for no break.

**Texts** are one file per field per language (`title.ru.txt`, `about.en.txt`,
`how_to_play.ru.txt`, ...). The name must be identical in the game, the draft
and the materials, and unique per language (5.2, 5.9); no field may repeat
another field's text or pad itself with repeated characters (5.8); controls are
described in "how to play" (2.2).

### 5. Fill the draft and read the pre-check

The console runs its own automated check on every archive and prints the
verdict above the form ("Замечания к релизу"). It is free, it is the same check
moderation starts from, and it answers in minutes — upload, read it, fix, upload
again. Two things the form itself decides: at most **two** categories, and a
draft cannot be submitted at all without a live RSYA contract on the account.

**The video slots cannot be filled from a browser agent.** The archive and the
screenshot widgets only ship a file to the server, so those go up fine. A video
widget first reads the clip's metadata through a `<video>` element on a blob
URL, and a file the browser did not pick through its own dialog is unreadable
to the renderer: the element stays at `readyState 0` forever, and the handler
returns without an error, a message or a request. A fresh tab behaves the same.
Plan for the lead to drag both mp4 files in per language.

The draft itself is plain JSON and worth knowing when the form fights back:
`GET /console/api/application/<id>` returns it under `draft`, and saving is
`PATCH /console/api/application-draft/<id>` carrying only the changed
top-level keys (`sources`, `screenshots`, `videos`) plus an `x-csrf-token`
header. Media inside that delta are bare file ids produced by
`POST /console/api/files/{screenshots,videos}` with fields `file` and `app-id`.
Video is the one thing not to attach this way: `options.orientation` is written
by the widget, cannot be passed to the upload, and a clip that lacks it is
never placed in a slot.

### 6. Hand over

The reply to the lead names: the archive path, the SDK probe result, the
checklist rows that are NOT closed and what each would take, and the store
folder with the checker's output. When videos are part of the delivery, name
the files and the slot each belongs to — that list is the lead's whole task.
Uploading the draft, the age rating, the categories and the platform flags are
the lead's — the console is theirs.

## What This Skill Does Not Do

- It does not press "Отправить на модерацию". Filling the draft is work; sending
  it is the lead's word, once per submission.
- It does not invent sizes. Numbers come from the form or from the spec file.
- It does not make screenshots out of promotional art, and does not make an
  icon out of a screenshot — both are refusals.
- It does not put video into the draft. It films the clips, checks them and
  hands them over; the slots are filled by hand.

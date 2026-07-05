---
id: T0315
title: "rb-dark-rpg polish: настройки, крестик, слайдеры и TG"
status: review
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, polish, settings]
created: 2026-07-05
updated: 2026-07-05
---

## What
Polish the settings screen frame, close button, sliders, and Telegram links for native PC and web builds.

## Done when

- [x] Settings frame/body padding is tighter.
- [x] Close button has a larger hit target and readable X without the bulky slice9 button art.
- [x] Sliders use real track/fill/thumb assets and keep the thumb visible on hover/drag.
- [x] Author Telegram URL is `https://t.me/d954mas_make_games`.
- [x] Link code supports web `window.open` and Windows native `ShellExecuteA`.

## Open questions

## Log
- 2026-07-05: Start: settings frame, close button, sliders, TG channel/buttons.
- 2026-07-05: Reduced settings frame padding, restored real slider assets/thumb, enlarged plain close button, and replaced author TG placeholder with https://t.me/d954mas_make_games. Links open via window.open on web and ShellExecuteA on Windows native.
- 2026-07-05: Verification: native game target built; native test suite passed. Settings TG URL is wired to the requested channel; web opens via window.open and Windows native via ShellExecuteA, but live click verification was blocked by DevAPI timeout.

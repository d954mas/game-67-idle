---
id: T0001
title: Web and mobile portrait playable build
status: backlog
epic: E001
priority: P1
tags: [web, mobile, portrait, validation]
created: 2026-06-13
updated: 2026-06-13
---

## What

After native proof passes, build and validate the Rune Marches slice for web
desktop and mobile portrait. Web work is allowed for this project because the
user explicitly requested "pc and web (pc and mobile)".

## Done when

- [ ] Exact web build command/preset is discovered from local CMake presets.
- [ ] Web build runs without replacing the native-first source of truth.
- [ ] Desktop browser screenshot shows readable map, stats, primary action,
  combat/upgrade state.
- [ ] Mobile portrait screenshot at 360 x 640 or equivalent shows reachable
  primary action and no critical overlap.
- [ ] Touch/click input path proves scout -> combat -> reward -> upgrade.
- [ ] Any web server start is preceded by restating the explicit user web
  permission.

## Open questions

- Which minimum device/profile should represent the first Poki mobile test?

## Log

- 2026-06-13: Backlog task created; blocked on native first playable proof.
- 2026-06-13: Existing `wasm-qa` preset was discovered and configure/build was
  briefly attempted after native proof, but the user questioned the web-build
  lane. Pause this task until web/mobile validation is explicitly the active
  lane again; continue native PC content/FTUE work meanwhile.

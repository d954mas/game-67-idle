---
id: T0002
title: Core loop reducer/model (no UI)
status: done
epic: E001
priority: P1
tags: [fantasy-pocket-rpg, core-loop]
created: 2026-06-11
updated: 2026-06-11
---

## What

Travel -> search -> loot -> camp -> craft/rest -> unlock next node works without UI.

## Done when

- [x] actions are deterministic
- [x] blocked states return readable reasons
- [x] first 5 minute path can be simulated

## Open questions

## Log

- 2026-06-11: Seeded from implementation_tasks.json phase list.
- 2026-06-11: Added `src/game_state_actions.c` and verified the map -> ruins -> combat -> camp -> unlock path with `py -3.12 tools/devapi/smoke_test.py 9123`.

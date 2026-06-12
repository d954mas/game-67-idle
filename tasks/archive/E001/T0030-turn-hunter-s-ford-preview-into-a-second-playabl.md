---
id: T0030
title: "Turn Hunter's Ford preview into a second playable route"
status: done
epic: E001
priority: P1
tags: [gameplay, map, combat, progression]
created: 2026-06-12
updated: 2026-06-12
---

## What

Turn the unlocked Hunter's Ford map node from a route preview into the first small second-node loop for Iteration 4.

Scope: travel to Hunter's Ford, resolve one ford scouting/search beat, fight one new enemy type, receive a small cache/faction/dragon clue, and return to the map.

Out of scope: full 8-12 node region, new art pack, shops, faction reputation system, second camp, and new UI framework.

## Done when

- [x] clicking unlocked `Hunter's Ford` enters a distinct encounter instead of only setting a preview flag
- [x] Hunter's Ford has a readable search/scout beat with a cache or clue reward
- [x] Hunter's Ford introduces a second enemy type with distinct HP/damage text
- [x] clearing the route sets persistent progress and returns to the province map
- [x] DevAPI smoke/playtest cover the second route after the first loop
- [x] native screenshot evidence shows the second route or its clear result
- [x] review pass records/fixes gameplay/code issues before moving on

## Open questions

## Log

- 2026-06-12: Started after native release-candidate refresh. This is the next concept-aligned product increment after the first Old Road loop.
- 2026-06-12: Done. Implemented Hunter's Ford as a second route with travel Supply pressure, river cache scout reward, Ford Bandit Scout combat, payoff/result screen, persistent clear state, and map next-track status.
- 2026-06-12: Review fixes from gameplay/code subagents: added distinct Ford visual motif and action id, tightened resource pressure, added payoff screen, clarified post-clear button copy, capped rewards, added action validation rollback, stricter Ford invariants, strict envelope validation, v0 migration load coverage, and generator schema coverage guard.
- 2026-06-12: Evidence: `cmake --build --preset game-native-debug`, `py -3.12 tools/devapi/scenarios/state_roundtrip.py 9165`, `py -3.12 tools/devapi/smoke_test.py 9166`, `py -3.12 tools/devapi/agent_playtest.py 9167 --full-loop --exe build/game_67_idle/native-debug/game_67_idle.exe --out-dir build/captures/t0030_hunter_ford_second_route_final`, `cmake --build --preset game-native-qa`, screenshot `build/captures/t0030_hunter_ford_second_route_final/screenshots/agent_hunter_ford_20260612_031846.png`.

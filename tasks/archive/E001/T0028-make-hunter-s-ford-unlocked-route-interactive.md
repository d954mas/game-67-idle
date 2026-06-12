---
id: T0028
title: "Make Hunter's Ford unlocked route interactive"
status: done
epic: E001
priority: P1
tags: [gameplay, map, progression, automation]
created: 2026-06-12
updated: 2026-06-12
---

## What

Make the newly unlocked `Hunter's Ford` route respond when clicked after the first camp beat. This is a route preview/scouting beat, not a second full encounter node.

Out of scope: new enemy, loot table, second camp, faction system, and new art.

## Done when

- [x] unlocked `Hunter's Ford` click no longer returns `Unknown action`
- [x] player gets a readable scouting/next-objective result
- [x] route remains a clear future hook and does not break the first loop
- [x] DevAPI smoke/playtest coverage clicks the unlocked route
- [x] native screenshot evidence shows the route-preview state

## Open questions

## Log

- 2026-06-12: Started from post-T0027 review. The map button is enabled after unlock and labeled `Travel`, but no `travel_hunter_ford` action exists yet.
- 2026-06-12: Done. Added `travel_hunter_ford` route preview, persistent `hunter_ford_scouted`, DevAPI smoke/playtest coverage, and screenshot evidence at `build/captures/t0028_hunter_ford_preview/screenshots/agent_hunter_ford_20260612_025415.png`.

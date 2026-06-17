---
id: T0004
title: "P2-P4: Casual RPG core loop (move, fight, clear keep, loot, level)"
status: done
epic: E001
priority: P2
tags: [voxelheim, gameplay, state]
created: 2026-06-16
updated: 2026-06-16
---

## What

After the first screen passes the visual gate (T0003), build the casual core
loop: tap/click-to-move + camera, 1-2 frost enemies with auto-attack combat and
win-on-keep, then loot + XP + a visible LEVEL UP wired to a persistent
`GameState`. Do NOT start during the visual-first freeze.

## Done when

- [x] Click/tap-to-move hero (single fixed screen, no camera follow needed);
      synthetic-input smoke probe (`input.click` + `game.debug.click`).
- [x] 3 ice-goblins, auto-attack in range, HP/death, win = clear all + enter keep.
- [x] XP → visible level-up (LEVEL UP! text + hero glow/scale pop + max-HP grows).
- [x] HUD (HP fill / stamina / level badge / quest banner) wired to generated
      `GameState.run.*`; persisted via `state/` schema v2 + `v1_to_v2` migration;
      DevAPI `game.state` / `game.reset_playtest` / `game.debug.click` / `.tick`.
- [x] Native playtest probe (`tools/devapi/voxelheim_play_test.py`) proves
      move→fight→clear→levelup→victory end-to-end (19/19 checks).

## Open questions

- Auto-attack vs one-button attack for "casual"? RESOLVED: auto-attack in range.

## Log

- 2026-06-16 created (depends on T0003).
- 2026-06-16 implemented P2-P4 core loop: movement, 3-enemy combat, XP/level-up,
  win-on-keep, FTUE (3 beats), juice (hit flash, floating damage, sparkles, glow,
  level-up pop). State schema v2 + run fields + migration. End-to-end DevAPI probe
  (19/19) with start/combat/levelup/victory screenshots, all pixel_health green.

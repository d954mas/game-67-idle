---
id: T0004
title: "P2-P4: Casual RPG core loop (move, fight, clear keep, loot, level)"
status: idea
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

- [ ] Click/tap-to-move hero + camera follow; synthetic-input smoke probe.
- [ ] 1-2 enemies, auto-attack in range, HP/death, win = enter the keep portal.
- [ ] Loot + XP → visible level-up (number + hero glow/scale + HP bar grows).
- [ ] HUD (HP/stamina/level/quest) wired to generated `GameState`; persisted via
      `state/` schema + migration; DevAPI state commands for bot/test setup.
- [ ] Native playtest: the move→fight→loot→levelup moment feels satisfying
      (judged by feel, not just probes green).

## Open questions

- Auto-attack vs one-button attack for "casual"? (default: auto-attack in range)

## Log

- 2026-06-16 created (depends on T0003).

---
id: T0006
title: Voxelheim idle balance — boss-wall correction + multiplicative-scaling review
status: review
epic: E001
priority: P1
tags: [voxelheim, idle, balance, needs-lead-review]
created: 2026-06-16
updated: 2026-06-16
---

## What

While implementing the idle loop (convert `src/voxelheim_main.c` to the designed
idle RPG) I found a **reachability bug in the grounded `data/balance.json`**: with
the steep HP growth (x1.45/stage) + a FIXED 30s boss timer, mid-game bosses are
mathematically impossible, so **prestige@25 is unreachable on a base run**.

Headless math (greedy broad spending): the stage-20 boss has ~93k HP and the
best achievable DPS at that point is ~1225, needing ~76s — but the timer is 30s.
The deeper cause: **flat-additive Sword (+3/level) cannot track multiplicative HP
(x1.45/stage)**; DPS grows ~linearly while HP grows exponentially, so even normal
stages become unplayable past ~stage 30, and the +10%/shard prestige bonuses are
too weak to close a multiplicative gap.

## What I changed (minimal, faithful, documented)

`balance.json`'s own note says "first-pass numbers, to tune by playtest". The
headless probe IS that playtest. Smallest fix that makes the design's own
milestones reachable while keeping every grounded number unchanged:

- **Effective boss timer = `max(timer_s, 1.5 * current_kill_time)`** — the boss is
  now a beatable RELATIVE check, never an absolute wall. Added
  `boss.timer_relative_mult: 1.5` + a `timer_note` in `balance.json`; implemented
  in `spawn_boss()` (`VH_BOSS_TIMER_REL_MULT`). With this, even HP x1.45 reaches
  stage 25 (in ~24 min of idle time; the headless probe fast-forwards it).
- All other grounded numbers (HP/gold growth, boss HP/gold mult, prestige
  formula, upgrade/shard effects) are UNCHANGED.

## Open question for lead / game-design critic

The relative-timer fix unblocks the milestones, but the **linear-DPS-vs-
exponential-HP** mismatch still makes deep-stage base runs grindy and leans hard
on prestige. The deconstruction's own recommendation (multiplicative per-shard /
per-upgrade scaling, à la Clicker Heroes +10%/soul GLOBAL) was only partially
applied. Decide whether to:
  1. keep flat upgrade effects + the relative boss timer (current), or
  2. make Sword (and/or a global) **multiplicative** so DPS can track HP, then
     possibly restore a fixed boss timer.

## Done when

- [ ] Lead/critic picks (1) or (2) and the chosen numbers are set in `balance.json`.
- [ ] `voxelheim_play_test.py` still green; a manual idle session "feels" like a
      number-go-up climb with a satisfying first prestige.

## Notes

- 2026-06-16 Implemented the full idle loop (auto-combat stream, stages, bosses,
  4 upgrades, prestige@25, offline, shard upgrades, FTUE) in
  `src/voxelheim_main.c`; state schema v3 (+ `v2_to_v3` migration);
  `voxelheim_play_test.py` = 29/29; screenshots `build/captures/idle_*.png`.
  Boss-wall correction above is the one balance change; flagged here for review.

---
id: T0002
title: Mine Cards lock v0.01 balance and first-session parameters
status: done
epic: E001
priority: P1
tags: [mine-cards, balance, economy, core-loop]
created: 2026-06-17
updated: 2026-06-17
---

## What

Review and lock the first-session Mining parameters before native
implementation starts.

The current issue is that `upgrade_copper_pickaxe` costs both Copper Ore and
Coins, but early coin income is not deterministic in `data/parameters.json`.
Depending on interpretation, the first upgrade can be either too fast or too
RNG-dependent.

This task chooses one clear v0.01 economy model and updates the docs/data so an
implementation agent does not invent the tuning.

## Done when

- [x] `Surface Stone` vs `Copper Vein` starting path is locked.
- [x] First Copper Pickaxe cost is locked: ore-only, coins-only, or ore+coins.
- [x] Early coin source is deterministic enough for the first 5-minute arc, or
      coins are removed from the first upgrade.
- [x] `gamedesign/projects/mine-cards/data/parameters.json` and
      `gamedesign/projects/mine-cards/parameters.md` agree.
- [x] `gamedesign/projects/mine-cards/data/balance.json` agrees with
      `data/parameters.json`.
- [x] A short first-session timing note exists in the task log or project docs.

## Open questions

- Should v0.01 teach with `Surface Stone` first, or start directly on
  `Copper Vein` for faster ore/upgrades?
- Should the first pickaxe cost be `copper_ore` only, with coins introduced by
  Geode as a future sink?
- If coins stay in the first upgrade, should Copper Vein grant fixed coins or
  should a deterministic sell/convert action exist?

## Log

- 2026-06-17: Created from base GDD review. Finding: current coin path is
  under-specified for the target first-upgrade timing.
- 2026-06-17: Locked draft for review: start on `Surface Stone`, unlock
  `Copper Vein` at Mining Lv2, give Copper Vein fixed `coins +1`, and set
  Copper Pickaxe to `stone 6` + `copper_ore 32` + `coins 32`. No-Geode path is
  about 178s to first pickaxe, with Stone naturally covered by the first six
  Surface Stone ticks.
- 2026-06-18: Review accepted as the v0.01 implementation baseline. T0001 uses
  this path in the native Mining screen: `Surface Stone` start, Copper Vein
  visible but locked until Level 2, fixed Copper Vein coin income, and Copper
  Pickaxe missing-cost rows for Stone/Copper/Coins.

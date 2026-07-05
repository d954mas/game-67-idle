---
id: T0292
title: "rb-dark-rpg: принять 30-минутный баланс и визуальный fake shot"
status: review
project: P003
epic: E012
priority: P1
tags: [rb-dark-rpg, release, balance, visual, gdd]
created: 2026-07-05
updated: 2026-07-05
---

## What

Review and accept or revise the 30-minute release balance contract and the
first gameplay fake shot for `games/rb-dark-rpg`.

## Done when

- [x] `games/rb-dark-rpg/design/release_30_minute_balance.md` defines the
  30-minute pacing, 10 levels, 10 critical quests plus optional support quests,
  5 gear tiers, 12 enemies, reward budget, and implementation order.
- [x] `games/rb-dark-rpg/design/data/release_balance.json` validates as JSON and
  contains 10 levels, 13 quests, 5 tiers, 12 enemies, and a 540 XP Act I cap.
- [x] `games/rb-dark-rpg/design/layout/release_30min_fake_shot.svg` and `.png`
  show the gameplay-readable visual target.
- [ ] Lead accepts the balance/visual direction or requests concrete changes.

## Open questions

- Should the visual copy stay English for the design proof, or should the next
  pass make the fake shot fully Russian-facing?
- Should optional quest XP allow overleveling beyond the 540 XP cap, or should
  level 10 remain a hard release cap?

## Log

- 2026-07-05: Added release balance contract, structured release balance JSON,
  SVG source, PNG preview, and visual notes. Evidence: JSON count check returned
  10 levels, 13 quests, 5 tiers, 12 enemies, final XP 540; SVG parsed as XML;
  `git diff --check` passed on the new release artifacts.

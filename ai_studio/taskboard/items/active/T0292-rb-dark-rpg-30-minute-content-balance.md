---
id: T0292
title: "rb-dark-rpg: accept 30-minute content balance"
status: review
project: P003
epic: E012
priority: P1
tags: [rb-dark-rpg, release, balance, content, gdd]
created: 2026-07-05
updated: 2026-07-05
---

## What

Review and accept or revise the 30-minute release balance/content contract for
`games/rb-dark-rpg`.

## Done when

- [x] `games/rb-dark-rpg/design/release_30_minute_balance.md` defines the
  30-minute pacing, 10 levels, 10 critical quests plus optional support quests,
  5 gear tiers, 12 enemies, reward budget, and implementation order.
- [x] `games/rb-dark-rpg/design/data/release_balance.json` validates as JSON and
  contains 10 levels, 13 quests, 5 tiers, 12 enemies, and a 540 XP Act I cap.
- [ ] Release content is expanded into authored game data files.
- [ ] Lead accepts the balance/content direction or requests concrete changes.

## Open questions

- Should optional quest XP allow overleveling beyond the 540 XP cap, or should
  level 10 remain a hard release cap?

## Log

- 2026-07-05: Added release balance contract and structured release balance
  JSON. Evidence: JSON count check returned 10 levels, 13 quests, 5 tiers,
  12 enemies, final XP 540; `git diff --check` passed on the new release
  artifacts.
- 2026-07-05: Removed generated fake-shot visual artifacts after lead clarified
  that the requested work is authored game content and gameplay assets, not
  mock visuals.

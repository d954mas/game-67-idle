---
id: T0287
title: "rb-dark-rpg: reward presentation and loot feedback pass"
status: backlog
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, combat, rewards, uiux, assets]
created: 2026-07-04
updated: 2026-07-05
---

## What

Polish reward presentation after combat so XP, gold, quest items, and gear are
visually distinct and state changes are obvious. This is the reward-feedback
follow-up after T0285 correctness and T0286 combat readability.

Scope:

- reward cells for XP, gold, stackable quest/progress items, and gear;
- icon/color hierarchy so rewards do not look identical;
- integration with result panel and follow-up HUD state;
- compatibility with the existing item/gear data model.

Out of scope:

- changing reward ids or deleting existing rewards without migration;
- full inventory art production for every future item.

## Done when

- [ ] Victory result uses distinct reward cells for XP, gold, item stacks, and
      gear rewards.
- [ ] Multi-reward encounters show every reward item clearly.
- [ ] Reward visuals are readable at small cell size and on phone portrait.
- [ ] Runtime assertions still prove XP/gold/items/gear/claimed reward ids match
      the visible result.
- [ ] Asset provenance is recorded for any committed generated/imported reward
      art.
- [ ] Subagent review covers reward readability and state/compatibility risk.

## Open questions

- Should this pass reuse placeholder vector/icon art first, or wait for the
  item art task `T0273` to supply production icons?

## Log

- 2026-07-05: Created as the reward-feedback slice after T0285/T0286.

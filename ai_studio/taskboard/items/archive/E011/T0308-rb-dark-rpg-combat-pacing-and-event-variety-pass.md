---
id: T0308
title: "rb-dark-rpg: combat pacing and event variety pass"
status: done
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, combat, pacing, ui]
created: 2026-07-05
updated: 2026-07-05
---

## What

Improve the running combat screen after the Loop Hero-style pass:
- remove simultaneous-feeling exchanges from the combat timeline;
- make the first hit land before a full attack interval;
- make crit/block/normal hits read differently in the central collision;
- keep miss/dodge out of v1 until accuracy/evasion or enemy archetypes own it.

## Done when

- [x] Combat tests cover opening offset and no same-timestamp exchange.
- [x] Running combat uses staggered event timing in both preview and resolved result.
- [x] Center stage differentiates normal hit, block, and crit without adding stat clutter.
- [x] Design docs/data describe the updated timing contract.
- [x] Native combat tests and gate responsive combat capture pass.

## Open questions

- Should v2 add dodge/miss as a named enemy/player archetype instead of global RNG?

## Log

- 2026-07-05: Started pacing/event-variety pass after visual review of simultaneous attacks.
- 2026-07-05: Implemented opening offsets, event spacing, crit/block visual reactions, and updated docs/tests.
- 2026-07-05: Verified `game_combat_test`, `first_scene_tests`, `game`, `scenarios_test.py`, and gate responsive capture. Mill running capture is too short for a stable all-viewport screenshot without a freeze-time fixture.
- 2026-07-05: Completed combat pacing and event-variety pass: staggered timeline, crit/block reactions, docs, tests, gate responsive evidence.

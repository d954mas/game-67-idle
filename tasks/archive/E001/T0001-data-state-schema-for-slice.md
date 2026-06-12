---
id: T0001
title: Data/state schema for slice
status: done
epic: E001
priority: P1
tags: [fantasy-pocket-rpg, state]
created: 2026-06-11
updated: 2026-06-11
---

## What

Game can store current screen, resources, stats, route unlocks, camp availability, inventory, and one upgrade.

## Done when

- [x] state fixture loads
- [x] reset starts at province_map
- [x] resource mutation is testable

## Open questions

## Log

- 2026-06-11: Seeded from implementation_tasks.json phase list.
- 2026-06-11: Implemented fantasy slice schema/codegen and verified with `py -3.12 tools/devapi/scenarios/state_roundtrip.py 9123`.

---
id: T0360
title: Refactor Items tooling around one strict catalog model
status: done
project: P001
epic: E015
priority: P1
tags: [items, tooling, validation]
created: 2026-07-10
updated: 2026-07-10
---

## What

Close the pre-Lua Items refactor card because E016 ratified modular Items Lua
as the sole canonical current definition source. Implementing this card's
strict JSON catalog model would recreate the dual truth the lead rejected.

## Done when

- [x] Canonical ownership is linked to completed decision T0369 and E016.
- [x] Explicit game context and compact CLI behavior remain owned by T0366.
- [x] Release history/storage compatibility remains owned by T0381.
- [x] One deterministic normalized model, provenance, and focused reads remain
      owned by T0383.
- [x] Reference-template parity, consumer cutover, tests, and old JSON/parser
      deletion remain owned by T0386.
- [x] Closure does not claim the old Items tooling refactor was implemented.

## Open questions

None. E016 is the only implementation plan for canonical Items authoring.

## Log

- 2026-07-10: Superseded after T0369 ratified single-source Items Lua.
- 2026-07-10: Quality: not-applicable; reason: stale planning card closed in
  favor of the reviewed E016 task chain.

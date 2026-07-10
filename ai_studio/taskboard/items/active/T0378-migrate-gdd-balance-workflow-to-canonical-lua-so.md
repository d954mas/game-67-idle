---
id: T0378
title: Migrate GDD balance workflow to canonical Lua sources
status: backlog
project: P001
epic: E016
priority: P1
tags: [balance, gdd, migration]
created: 2026-07-10
updated: 2026-07-10
---

## What

Remove the old GDD route that makes implementation-driving numbers look
canonical outside Balance Lua while preserving prose design intent in the GDD.

## Done when

- [ ] `nt-primary-gdd`, `gameplay_systems.md`, GDD templates, handoff docs, and
      examples say that formulas, tunable numbers, scenarios, measurable
      targets, and requirements live in game-owned Balance Lua.
- [ ] GDD retains intent, player experience, pillars, rationale, and links to
      inspected Balance views/results rather than copying mutable values.
- [ ] `data/combat.json` and every live router/reference to it are removed or
      migrated; repository search finds no second canonical balance source.
- [ ] Existing games without Balance are handled by an explicit migration/error
      message, not silent fallback.
- [ ] Documentation remains a package of focused files, not one giant Markdown
      document.

## Open questions

## Log

- 2026-07-10: Split from UI work because source-of-truth migration must complete
  even if the exact Workbench v1 screen remains undecided.

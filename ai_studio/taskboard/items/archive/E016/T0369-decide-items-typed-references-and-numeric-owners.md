---
id: T0369
title: Ratify single-source Items Lua ownership and runtime boundaries
status: done
project: P001
epic: E016
priority: P0
tags: [items, lua, decision]
created: 2026-07-10
updated: 2026-07-10
---

## What

Decide whether item definitions remain split between Items JSON and Balance Lua
or become one modular Lua source, then classify generated runtime data, Viewer,
save state, and release history without dual truth.

Accepted concept:
`features/items-core/docs/items_lua_single_source_concept_2026-07-10.md`.

## Done when

- [x] References, rename/deletion, diagnostics, serialization, levels, and
      agent authoring are demonstrated without duplicate ownership.
- [x] The lead chooses complete game-owned Items Lua as the canonical current
      definition and separate implementation/migration tasks are updated.
- [x] `base_value`, numeric `use.params`, stack, container capacity, currency
      cap, item stats, formulas, and level tables are classified.
- [x] Conflicting future T0316/E016 clauses are amended; no migration may retain
      JSON and Lua as simultaneous fallback sources.

## Open questions

None in the ownership decision. The generated stable-core/typed-block include
seam is an implementation gate in T0364.

## Log

- 2026-07-10: Earlier reviews accepted opaque `items.ref` but left complete
  content/numeric ownership open.
- 2026-07-10: Lead rejected two-place Items ownership and chose modular Items
  Lua as the sole source. Viewer is read-only v1; snapshot and C are generated;
  save state and release history stay separate because they describe different
  facts.
- 2026-07-10: Architecture, adversarial, and official-source competitor reviews
  returned ACCEPT-WITH-FIXES. The final concept uses explicit stable field IDs,
  fixed L1 core plus generated typed blocks, strict unique-level semantics,
  deterministic full evaluation, cost lists, level overrides, and one-shot
  JSON/schema cutover.
- 2026-07-10: Single-source Items Lua decision ratified after architecture, adversarial, and competitor review; implementation gates routed to E016 tasks.

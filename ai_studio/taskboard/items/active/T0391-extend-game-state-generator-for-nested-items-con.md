---
id: T0391
title: Extend Game State generator for nested Items container state
status: doing
project: P001
epic: E019
priority: P1
tags: [game-state, generator, items, serialization]
created: 2026-07-10
updated: 2026-07-16
---

## What

Add the smallest reusable Game State generator capabilities required for the
accepted Items container state: exact u32 IDs, bounded nested object
collections, flat bounded C pools, and nested JSON/DevAPI projection.

## Done when

- [ ] Schema supports exact `u32` with zero/min/max constraints and generated C,
      JSON, validation, fixtures, and DevAPI round-trip without double/string
      ambiguity.
- [ ] Schema can express bounded `containers[].entries[]` object collections
      using schema-v2 stable paths and reserved removed names; the task does not
      introduce a numeric-field-ID schema-v3 dialect.
- [ ] Generated runtime storage uses separate bounded container and entry pools,
      not `MAX_CONTAINERS * MAX_ENTRIES_PER_CONTAINER`, while JSON/DevAPI nests
      each entry under exactly one container.
- [ ] Nested list projection supports a small schema-declared primitive-field
      canonical order so Items can serialize containers by ID and entries by
      slot/ID without depending on pool order or a handwritten serializer.
- [ ] Generated parser enforces structural type/range/collection budgets and
      rejects truncated or excessive nested input before publishing state.
      Items-specific global ID/slot uniqueness and ownership relations remain
      T0390 domain validation unless reusable declarative constraints are added.
- [ ] Generator tests prove empty/max/sparse containers, deterministic output,
      schema evolution, and Windows/Linux parity without hand-edited generated C.
- [ ] T0390 can declare its complete target schema using only supported schema
      constructs and domain actions rather than a bespoke second serializer.

## Open questions

- Keep this extension narrowly bounded to the first proven nested aggregate;
  do not introduce arbitrary recursive schemas or heap-backed collections.

## Plan

1. Add exact `u32` schema/codegen/JSON-helper support with focused RED tests.
2. Add one bounded schema-v2 `list<Object>` aggregate shape with separate top-
   level and nested pools; reject deeper recursion and unsupported shapes.
3. Project the pools as nested JSON in schema-declared primitive `order_by`
   order and parse into staged state under explicit collection budgets.
4. Prove the complete T0390 schema as a generator fixture, then run generator,
   native round-trip, template-build, and changed-domain verification.

## Log

- 2026-07-14: Moved to E019 as the first runtime-v2 prerequisite. It no longer
  blocks Items Lua authoring or JSON cutover.

- 2026-07-10: Created after review proved the current generator supports neither
  `u32` nor nested object collections. This is a prerequisite for T0390, not an
  optional cleanup.
- 2026-07-16: Plan review ACCEPT. Keep the extension to exact `u32` plus a
  depth-two `list<Object>` aggregate; preserve existing map/list behavior and
  reject arbitrary recursion, heap storage, numeric field IDs, and Items domain
  invariants in the generator.

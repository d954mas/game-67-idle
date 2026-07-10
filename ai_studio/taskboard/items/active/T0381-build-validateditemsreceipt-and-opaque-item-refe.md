---
id: T0381
title: Build phased Items registration refs and release compatibility receipt
status: backlog
project: P001
epic: E016
priority: P0
tags: [items, lua, validation, migration]
created: 2026-07-10
updated: 2026-07-10
---

## What

Implement the evaluator phases that register schema/IDs before resolving typed
references, and extend release compatibility checks beyond removed item IDs.

## Done when

- [ ] Evaluation freezes core/schema, registers kinds/all `def_id`,
      then resolves refs and formulas independent of module order.
- [ ] `items.ref(id)` is an immutable evaluator handle; missing/removed refs fail
      at the referring Lua source. Runtime containers are not evaluator refs.
      C gameplay uses generated checked hash constants; save/migration/DevAPI
      retain stable strings and blob indices remain fingerprint-private.
- [ ] The release receipt fingerprints shipped/removed `def_id`, stable
      `field_id`, storage mode, shipped `level_count`, state schema, and tool/API
      versions without duplicating current definitions.
- [ ] The existing `items.lock.json` is retained and extended in place as that
      one release receipt; no parallel receipt or import/delete transition can
      create a compatibility-history gap.
- [ ] Before any Lua/storage edit, a receipt schema bump deterministically
      augments the frozen old lock using the frozen pre-cutover catalog to seed
      field IDs, storage mode, and shipped level bounds; a fixture proves the
      exact v1-lock -> extended-receipt bytes and all later runs are no-op.
- [ ] Runtime enforces stack versus unique API routes; levelled data is allowed
      only for unique instances in v1. Stack APIs reject `stack == 1`, instance
      APIs reject `stack != 1`, and `stack > 1` is the actual enforced cap.
- [ ] Explicit tables derive max level from contiguous keys; generated/mixed
      modes require `max_level`. Row cost means `cost_to_reach` that target,
      level 1 is inapplicable, and level 2+ must be paid or explicit free.
- [ ] `acquire.cost` is separate from level cost; persistent unlock state is not
      implied. Composite costs accept only stackable resources, normalize
      duplicate items with checked sums, and route actor-specific sources through
      T0388 runtime payment scope.
- [ ] Decreasing shipped level bounds or changing storage requires an explicit
      migration/reaction; saved out-of-range levels never silently clamp.
- [ ] Forward refs, duplicate refs/IDs, removal/restoration, field rename/remove,
      storage change, and level shrink have deterministic fixtures.

## Open questions

- Dense runtime indices and hash lookup strategy remain a profiling decision;
  generated item hashes plus saved strings are the accepted public boundary.

## Log

- 2026-07-10: Re-scoped from validating an external Items JSON receipt. Items is
  now registered inside the shared Lua evaluator; the receipt remains necessary
  for historical compatibility rather than current-data duplication.

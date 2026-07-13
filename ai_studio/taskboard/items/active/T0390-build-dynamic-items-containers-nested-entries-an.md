---
id: T0390
title: Build dynamic Items containers and nested runtime entries
status: backlog
project: P001
epic: E016
priority: P0
tags: [items, containers, state, runtime]
created: 2026-07-10
updated: 2026-07-10
---

## What

Build game-created runtime container instances and persist their aggregates
with ordered nested entries, globally unique numeric IDs, and mutable capacity.
Legacy state/reference migration and event-consumer cutover are isolated in
T0392.

## Done when

- [ ] Concrete `backpack`/`purse` definitions and `items_purse()` assumptions
      leave reusable catalog/core; games create persistent or ephemeral
      containers through one runtime API.
- [ ] Persistent `uint32_t` container/entry IDs are unique within one save,
      reserve zero and `UINT32_MAX`, persist separate `last_container_id` and
      `last_entry_id`, allocate only `1..UINT32_MAX - 1` without wrap/reuse,
      serialize exactly as numbers, and remain distinct from runtime
      index+generation refs.
- [ ] State owns `containers[].entries[]`: every entry is nested in exactly one
      container, has a unique `entry_id` and explicit unique `slot`, and does
      not duplicate `container_id`.
- [ ] Canonical serialization orders containers by `container_id` and entries by
      `(slot, entry_id)`, independent of dense-pool allocation/deletion order.
- [ ] Depends on T0391: generated C uses separate bounded container/entry pools
      while JSON/DevAPI projects the accepted nested aggregate.
- [ ] Stack entries own counts; unique entries own per-instance level/
      durability. Whole-entry move preserves ID, split creates an ID, and merge
      retains destination ID and destroys source ID atomically.
- [ ] Runtime rebuilds derived `entry_id -> container/slot` indices after load;
      duplicate IDs/slots, missing containers, impossible counts, and invalid
      persisted counters reject the staged load. A structurally valid entry with
      missing/removed `def_id` alone is quarantined in place, still occupies its
      slot, is excluded from gameplay, and restores if the definition returns.
      Live allocation exhaustion refuses before mutation and never wraps.
- [ ] Capacity is addressable slots: `slot < capacity`, auto placement is first
      free, and shrink requires `max_occupied_slot < new_capacity`. V1
      has no unlimited sentinel (`capacity = 0` means zero slots). Policy is
      built-in/serializable and immutable after create. Destroying a non-empty
      container requires an explicit transfer/drain domain action.
- [ ] Persistent and ephemeral lifetimes cannot silently cross: persistent ->
      ephemeral move is rejected; ephemeral -> persistent acquisition creates a
      new persistent entry ID. Ephemeral objects have runtime refs only; asking
      them for persistent IDs asserts.
- [ ] Owner fragments store only `container_id`; Items does not duplicate owner
      pointers. Domain create/destroy updates both in memory before one envelope
      save, and raw DevAPI ownership writes or fragment resets that would break
      integrity refuse. Template default rejects a staged load with dangling
      owner refs or unreferenced persistent containers before publish; game
      recovery requires separate explicit versioned policy and fixtures.
- [ ] Missing required objects or invalid handles assert. Expected capacity,
      policy, occupied-slot, and exhaustion refusals use bounded `can/try`
      results and never partially mutate state.
- [ ] Fixtures cover player inventory, 100 merchant containers, persistent and
      ephemeral chests, two stacks of one definition, two unique swords with
      different durability, reorder/move/split/merge/resize/save/load.
- [ ] Runtime inspection is bounded: paginated/filterable container listing,
      one-container inspection with explicit entry range/filter, and hard
      row/byte/context budgets.

## Open questions

- Select conservative game-configurable maximum container/entry budgets from
  benchmarks rather than baking the current 64-entry template limit into core.
- Owner state stores only `container_id`; registry plus owner references commit
  in one save envelope and game-owned create/destroy actions update both. The
  strict template default is whole-operation/load refusal, not silent repair.

## Log

- 2026-07-10: Created after lead rejected fixed Lua/catalog containers. Runtime
  containers are game entities managed by Items invariants; nested state is the
  canonical persistence shape and flat lookup tables are derived caches only.
- 2026-07-10: Split legacy migration/reference/event cutover into T0392 after
  red-team review; T0390 now owns only the new runtime/state contract.

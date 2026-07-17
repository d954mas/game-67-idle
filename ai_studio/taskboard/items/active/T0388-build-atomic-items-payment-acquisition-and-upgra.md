---
id: T0388
title: Build atomic Items payment acquisition and upgrade verbs
status: doing
project: P001
epic: E019
priority: P1
tags: [items, runtime, state, transaction]
created: 2026-07-10
updated: 2026-07-17
---

## What

Provide generic all-or-nothing runtime verbs for normalized composite payment,
item acquisition, and unique-instance level upgrades. Game policy decides when
an action is offered; Items owns catalog/state/storage invariants and commit
atomicity.

## Done when

- [ ] `items_try_pay_cost` accepts one normalized resource-only cost plus an
      explicit ordered bounded runtime payment scope. Scope overflow or duplicate
      source containers assert as developer errors. It creates a deterministic plan,
      consumes containers in scope order and compatible stacks by ascending
      slot, preflights every source entry, then commits all or nothing.
- [ ] Duplicate item requirements are canonicalized by export. Malformed,
      overflowing, or non-positive entries in the opaque normalized runtime cost
      are corrupt-catalog/developer assertions, never gameplay refusal. Runtime
      never performs a partial loop of public remove calls or hidden global search.
- [ ] `items_try_acquire` pays `acquire.cost` and creates/adds the definition in
      an explicit destination container as one commit; failed capacity, policy,
      uniqueness, payment, or creation leaves state unchanged.
- [ ] If destination is also a payment source, acquire applies the complete
      payment plan to projected state before planning placement/merge/ID
      allocation, then commits the combined plan once.
- [ ] Missing `acquire.cost` makes generic acquisition unavailable; free
      acquisition is accepted only when authored explicitly as `items.free()`.
- [ ] `items_try_upgrade_instance(item_entry_ref_t entry, target_level, payment, reason)`
      requires the next valid level in v1, obtains `cost_to_reach[target_level]`,
      pays, changes saved level, marks dirty, and emits one result atomically.
- [ ] Explicit free and paid transitions are distinct; level 1/inapplicable is
      never treated as free. Persistent unlock state is outside this task.
- [ ] Success/failure fixtures cover two explicit payer containers, duplicate
      resources, overflow, insufficient second resource, full/wrong-policy destination,
      wrong storage route, stale level, and injected commit failure with exact
      before/after state equality.
- [ ] The implementation preserves existing reason-tag/event/audit contracts
      and exposes one bounded failure reason rather than intermediate events.
- [ ] Invalid handles, missing required objects, duplicate/oversized payment
      scope, and malformed normalized cost assert. Only expected funds, capacity,
      policy, slot, unavailable acquisition, or pool exhaustion use bounded
      `can/try` refusal enums without partial mutation.

## Open questions

- Choose the smallest rollback-safe internal state mutation seam; do not expose
  a generic public transaction system unless the three verbs prove it necessary.
- Trading existing merchant stock remains Shop/game orchestration over atomic
  payment plus transfer; it is not catalog acquisition.

## Log

- 2026-07-14: Moved to E019. Atomic payment/acquisition is valuable runtime work
  after catalog costs and containers exist, not a Workbench blocker.

- 2026-07-10: Created from independent red-team/runtime reviews after composite
  costs exposed that sequential `can_afford`/`remove` calls cannot guarantee an
  atomic upgrade or acquisition.
- 2026-07-17: Started after T0390/T0391 closure because Taskboard ready queue ranks this P1 E019 task first; catalog costs and runtime containers are present. Work proceeds incrementally from payment planning tests.
- 2026-07-17: Implemented the first payment slice: bounded ordered scope,
  deterministic source/slot planning, exact no-mutation refusal, one commit,
  and a truthful typed `items.payment` summary event. Focused Items,
  progression, curve, and template-composition native tests pass; independent
  review findings on event semantics and slot-order evidence were addressed.

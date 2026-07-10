---
id: T0392
title: Migrate legacy Items state references and event consumers
status: backlog
project: P001
epic: E016
priority: P0
tags: [items, migration, state, events, cutover]
created: 2026-07-10
updated: 2026-07-10
---

## What

After T0391 and T0390, migrate the legacy flat Items ownership/fixed-container
state into nested numeric container aggregates and cut persistent references
and transient event consumers to the new identity model.

## Done when

- [ ] Migration uses a frozen game-owned mapping from each legacy container
      string to numeric ID, capacity, and policy; it never reads mutable current
      catalog or balance values.
- [ ] Every legacy `capacity = 0` unlimited container maps to a reviewed finite
      capacity in that frozen mapping; the new zero-slot meaning is never
      inferred from the old sentinel.
- [ ] Legacy map keys are sorted before assigning deterministic entry IDs and
      slots. Migration writes `last_container_id`/`last_entry_id` as the exact
      assigned maxima, rejects a reserved-maximum/exhausted assignment before
      publish, and repeated runs produce byte-identical migrated state.
- [ ] Persistent external owner references migrate in the same save document,
      through a versioned document-level stage that parses and migrates Items
      plus every affected owner fragment before publishing any of them. Failure
      rejects the whole staged document with exact before/after equality; no
      fragment is independently reset or committed.
- [ ] Template default rejects dangling owner references and unreferenced
      persistent containers before publish. A game may opt into a separately
      tested explicit migration/recovery policy, but core never clears, invents,
      destroys, or reattaches ownership silently.
- [ ] Event payload IDs and every producer/consumer are changed as a code/API
      cutover with compatibility tests; transient events are not treated as save
      rows to migrate.
- [ ] Raw DevAPI writes cannot create half-updated ownership relationships;
      supported domain operations update both fragments.
- [ ] Frozen v1 fixtures cover flat `owned`, fixed names, missing/removed item
      definitions, external references, deterministic order, and rollback/fail
      behavior, including one owner-fragment migration failure with exact
      unchanged document state, without hand-editing generated state C.
- [ ] A well-formed missing/removed definition quarantines the unchanged entry
      and keeps its slot/capacity; structural corruption rejects the whole staged
      document. Restoration of a compatible definition unquarantines it.

## Open questions

- Games need an override only if strict whole-document rejection is not suitable;
  such recovery is explicit versioned migration code with its own fixtures.

## Log

- 2026-07-10: Split from T0390 after red-team review separated new runtime
  implementation from old-save migration and transient event API cutover.

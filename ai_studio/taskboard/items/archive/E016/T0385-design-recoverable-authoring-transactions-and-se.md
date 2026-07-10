---
id: T0385
title: Close obsolete cross-owner Items and Balance authoring transactions
status: done
project: P001
epic: E016
priority: P1
tags: [balance, items, transactions]
created: 2026-07-10
updated: 2026-07-10
---

## What

Record the decision on the proposed recovery coordinator for edits that would
have touched Items JSON and Balance Lua together.

## Done when

- [x] Determine whether v1 has multiple canonical owners for one item edit.
- [x] Close or retain the transaction coordinator based on that ownership.
- [x] Preserve ordinary single-source expected-hash/diff/undo requirements as
      separate future literal-editing concerns.

## Open questions

None. A new transaction task may be created only for a demonstrated operation
that genuinely has multiple canonical owners.

## Log

- 2026-07-10: Intentionally closed. Single-source Items Lua removes the former
  Items JSON + Balance Lua transaction. Building lock/journal/crash recovery for
  a nonexistent v1 multi-owner edit would be unnecessary complexity.
- 2026-07-10: Intentionally closed: one canonical Items Lua source removes the proposed Items JSON plus Balance Lua cross-owner transaction.

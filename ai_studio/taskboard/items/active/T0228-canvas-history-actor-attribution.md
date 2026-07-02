---
id: T0228
title: "Canvas: history actor attribution - mark agent-made ops (robot icon)"
status: review
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead request during T0204 live look (2026-07-03): "в истории бы хотелось
как-то обозначить если сделал агент. Иконка/эмодзи робота".

Journal entries carry no actor today. Add transport-level attribution:

- `actor` field on every journal entry, set by the CLIENT transport at the
  commitMutation seam: CLI (`cli.mjs`) = `"agent"`, page HTTP (`api.mjs`) =
  `"user"`. Direct ops imports default to `"user"` unless the caller sets
  the actor (module-level setter, e.g. `setOpsActor()`, called once by
  cli.mjs at boot). No per-op signature churn.
- `listHistory` rows expose `actor`; the history panel prefixes agent rows
  with 🤖 (plain, no tooltip needed v1); CLI `history-list` shows the same
  marker (parity - identical labels via the shared historyEntryLabel).
- Old entries without the field render unmarked (no migration).
- Storage format: additive optional field only; journal stays append-only
  plain JSONL.

Coordinate with T0202 (warm worker) landing order - both touch ops.mjs;
this lands AFTER T0202 is accepted.

## Done when

- [x] journal entries record actor (cli=agent, page=user, additive field)
- [x] history panel shows 🤖 on agent-made entries; CLI history-list shows the same marker (marker lives IN the shared label - parity by construction, zero page changes)
- [x] old entries (no field) render unmarked; tests + gates green (226)

## Open questions

## Log
- 2026-07-03: Created from lead request during live verify of T0204.
- 2026-07-03: DONE by orchestrator (small seam, one writer on ops.mjs after T0202). setOpsActor("user"|"agent") module-level in ops.mjs; commitMutation records actor on every mutation entry (additive; absent = user for pre-T0228 lines). cli.mjs sets agent INSIDE the isMain guard only - importing the module (tests do) must not flip the process actor. listHistory row carries actor AND prefixes the label "🤖 " for agent rows - page palette + CLI render identical text with zero page changes. Base row actor=user. Tests: journal.test.mjs actor attribution + cli.test.mjs parity now pins CLI=agent labels. Tests 226.

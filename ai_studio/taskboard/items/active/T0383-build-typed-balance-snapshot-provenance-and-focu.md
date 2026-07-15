---
id: T0383
title: Build typed Items Snapshot provenance and focused dependency queries
status: backlog
project: P001
epic: E016
priority: P0
tags: [items, balance, snapshot, dependencies]
created: 2026-07-10
updated: 2026-07-15
---

## What

Normalize evaluator output into one logical Items/Balance Snapshot with schema,
source provenance, typed blocks, materialized levels, and bounded focused reads
for C export, Viewer, requirements, and agents.

## Done when

- [ ] Snapshot has deterministic order/hash, normalized values, stable field IDs,
      source spans, units/ranges, typed refs, recorded actual dependencies,
      level overrides, and runtime export metadata.
- [ ] Physical storage may be indexed/sharded while preserving one logical
      snapshot; consumers do not load large series by default.
- [ ] Focused queries support item/field/level-range plus inputs/dependents and
      return bounded stable JSON.
- [ ] Chart queries disclose bounds/downsampling; semantic diff compares two
      snapshots without creating a new source of truth.
- [ ] Source output promises location/span/snippet and computed rows, not a
      fabricated symbolic representation of arbitrary Lua closures.
- [ ] Pure named requirements produce stable severity and structured evidence,
      run without manually duplicated dependency truth, and support an explicit
      reviewed waiver record.
- [ ] Normalization rejects non-finite/unsafe values and preserves stable source
      locations/codes; deterministic math behavior is proven on Windows/Linux.
- [ ] Tests distinguish authoring rows, runtime-baked rows, chart points, and
      the 1000-row default. Larger stress sizes are selected by T0380 profiling.

## Open questions

- Select shard/index format only after benchmark evidence.

## Log

- 2026-07-14: Absorbed the bounded requirements/diagnostics outcome from T0384;
  sandbox limits remain T0382 and runtime conversion remains T0365.

- 2026-07-10: Re-scoped from joined JSON/Balance projection to the single-source
  Items Snapshot and honest formula/source capabilities.
- 2026-07-15: Slice 1: added deterministic items.snapshot.v1 build plus bounded item/field/level query with derived inputs and dependents; remaining provenance, typed schema, diff, chart, requirements, and cross-platform work stays open.
- 2026-07-15: Quality: QTECH_001=pass; evidence: items_snapshot_test 5/5, items_lua_sandbox_test 9/9, feature contracts pass, evaluator fixture query returned levels 2-3 and game.gold input.
- 2026-07-15: Slice 1 review: ACCEPT after structured malformed-snapshot and CLI-argument diagnostics were added; items_snapshot_test now 6/6.
- 2026-07-15: Quality: QTECH_001=pass; evidence: reviewed items_snapshot_test 6/6, sandbox 9/9, feature contracts 8/8, Taskboard validation pass, evaluator fixture query levels 2-3/game.gold input.
- 2026-07-15: Slice 2: evaluator now records each items.define Lua file/line; Snapshot keeps definition provenance outside content_hash and focused query returns it. Field/row spans and snippets remain open.
- 2026-07-15: Quality: QTECH_001=pass; evidence: provenance review ACCEPT, lua sandbox 10/10, Snapshot 6/6 including hash/source and malformed-source cases, feature contracts pass, fixture query returned weapons.lua:12.
- 2026-07-15: Slice 3: added bounded semantic Snapshot diff with stable item-relative JSON Pointer paths; provenance-only movement is ignored and default output stops above 1000 changes. Chart/downsampling remains open.
- 2026-07-15: Quality: QTECH_001=pass; evidence: semantic-diff review ACCEPT; Snapshot 8/8 covers CLI, stable paths/order, source-only no-op, type changes, add/remove, and 1001-change bound.
- 2026-07-15: Paused after Snapshot query/provenance/diff slices; T0381 registration and typed-ref prerequisite selected before remaining typed schema/chart/requirements work.

---
id: T0383
title: Build typed Items Snapshot provenance and focused dependency queries
status: backlog
project: P001
epic: E016
priority: P0
tags: [items, balance, snapshot, dependencies]
created: 2026-07-10
updated: 2026-07-10
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
- [ ] Tests distinguish authoring rows, runtime-baked rows, chart points, 1000
      default materialization, and 1M evaluator stress.

## Open questions

- Select shard/index format only after benchmark evidence.

## Log

- 2026-07-10: Re-scoped from joined JSON/Balance projection to the single-source
  Items Snapshot and honest formula/source capabilities.

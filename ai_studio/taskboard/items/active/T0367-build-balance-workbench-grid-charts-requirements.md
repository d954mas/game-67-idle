---
id: T0367
title: Specify developer and AI Items Workbench editing grid charts and what-if
status: backlog
project: P001
epic: E016
priority: P1
tags: [items, balance, ui, charts]
created: 2026-07-10
updated: 2026-07-10
---

## What

Retain the existing read-only Viewer as the migration slice, then specify the
target Workbench where developer UI and AI share semantic edits of safe Lua
shapes plus tables, charts, diff, source navigation, and what-if.

## Done when

- [ ] One fixed sword and explicit/generated/mixed levelled examples show
      identity, typed blocks, literals, target-level composite costs, explicit
      free transitions, overrides, diagnostics, release state, and source links
      without browser math.
- [ ] Level grid and synchronized chart support selected series, units, axis/
      interpolation metadata, bounded queries, and explicit downsampling.
- [ ] Semantic diff shows before/after snapshot changes and affected requirements.
- [ ] Ephemeral what-if parameters are visibly temporary, never build inputs,
      and can only produce a reviewed Lua patch or `Edit with agent` request.
- [ ] Inspector/grid edits recognized literals, table cells, curve parameters,
      and overrides through T0366 semantic ops with expected hash, preview diff,
      undo, conflict handling, and validation before commit.
- [ ] Unsupported expressions and arbitrary functions are visibly read-only and
      route to source/agent editing without replacing formulas with literals.
- [ ] Computed cells expose a separate `Create override` action; Snapshot fields
      carry `write_capability`, `read_only_reason`, and exact source range.
- [ ] Viewer labels source and computed values honestly; arbitrary closure text is
      not presented as a parsed symbolic formula.
- [ ] Items-only fallback, empty/error/loading states, keyboard navigation, copy,
      and source opening are specified and tested on Windows browser runtime.
- [ ] Copy/paste/fill/multi-row table operations are scoped explicitly and call
      one atomic semantic batch op with explicit `level` and composite
      `cost:<def_id>` columns, source+semantic diff, and inverse patch;
      persistent scenario history remains separate.
- [ ] LuaLS stubs generated from the same schema provide IDE types/autocomplete;
      schema owns metadata while views own only layout/order by stable field ID.

## Open questions

- Choose interactive row/chart budgets from focused-query measurements, not the
  1M evaluator stress workload.

## Log

- 2026-07-10: Lead selected read-only Viewer for the initial migration slice.
  Competitor review added level overrides, semantic diff, and ephemeral what-if.
- 2026-07-10: Lead clarified the product serves developers and AI. Restricted
  semantic editing of literals/tables/curves/overrides is now required target
  behavior; arbitrary Lua rewriting remains rejected.

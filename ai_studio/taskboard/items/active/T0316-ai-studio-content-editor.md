---
id: T0316
title: "AI Studio: Items Workbench поверх Lua Snapshot"
status: backlog
project: P001
epic: E009
priority: P2
tags: [viewer, items, tooling]
created: 2026-07-05
updated: 2026-07-10
---

## What

Maintain the accepted Studio Items Viewer and migrate its backend from the
current JSON op-layer to the focused single-source Items Lua Snapshot after E016
proves the evaluator/C boundary.

Current phase 1 is a read-only browser surface with registered game/template
selection, icons, schema-driven rows, filters, inspector, diagnostics, and lock
status. It does not own a second Items model.

The first Lua/Snapshot migration slice remains read-only. The target Workbench
then adds restricted semantic editing shared with AI:

- complete item/core/typed-block projection;
- level grid and selected-series charts;
- generated values, overrides, source span/snippet, dependencies, diagnostics,
  release state, and checked source navigation;
- semantic snapshot diff and visibly ephemeral what-if;
- literal/table/curve/override editing through T0366 operations;
- `Edit with agent` or source for arbitrary Lua, never guessed rewriting.

The full contract is E016 plus
`features/items-core/docs/items_lua_single_source_concept_2026-07-10.md`.

## Done when

- [ ] Viewer shows the catalog of every registered game/template with icons and
      no per-game JavaScript data model.
- [ ] After T0386 it reads bounded focused Snapshot queries from Items Lua; no
      consumer parses `items.json` or recalculates game math.
- [ ] Typed blocks, levels, overrides, cost lists, diagnostics, release state,
      source links, grid/chart, semantic diff, and what-if render from generated
      schema/snapshot metadata.
- [ ] What-if data is never a build input and can only become a reviewed Lua
      patch or `Edit with agent` request.
- [ ] Developer UI and AI share semantic literal/table/curve/override edits with
      expected hash, diff, undo, conflict refusal, and full validation.
- [ ] Items-only empty/error/loading behavior, path boundaries, icon resolution,
      and Windows browser tests remain green through cutover.

## Open questions

- T0364 must prove the stable-core/generated-typed-block contract before the UI
  fixes a representation for arbitrary game fields.
- T0367 specifies exact chart/grid/diff/what-if interaction and payload budgets.
- T0386 owns the one-shot JSON/schema/parser deletion and backend switch.
- T0366/T0367 own restricted semantic editing; arbitrary Lua write-back stays
  out of scope.

## Log

- 2026-07-08: Phase 1 accepted. Build spec:
  `ai_studio/assets/items_viewer/docs/build_spec_phase1_2026-07-08.md`.
  Implementation commits: `3789e24bc` spec/review fixes, `065199aaf` ops/tests,
  `4aea8c680` API/mounting, `458af3a33` page/README. Follow-up icon work linked
  compiled pack previews and retained read-only failure/path boundaries.
- 2026-07-08: UX direction accepted as table-first master/detail in Canvas
  visual idiom; containers and kinds stay visible domain concepts.
- 2026-07-10: Old generic `items.json` write phases were superseded. T0369
  ratified complete single-source Items Lua after architecture, adversarial, and
  competitor review. The accepted read-only surface stays; its next slice moves
  to T0367 and backend cutover to T0386.
- 2026-07-10: Lead clarified the product serves both developers and AI. The
  read-only surface remains the migration slice, not the final product; safe
  literal/table/curve/override editing must use the same semantic ops as AI.
- 2026-07-11: T0375 status reconciliation: moved stale doing to backlog. Phase 1 is accepted, but all six current cutover/Workbench criteria remain pending on E016 T0366/T0367/T0386; E016 is outside E015 execution scope.

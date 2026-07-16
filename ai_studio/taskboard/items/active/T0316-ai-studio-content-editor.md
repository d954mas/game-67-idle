---
id: T0316
title: "AI Studio: Items Workbench поверх Lua Snapshot"
status: doing
project: P001
epic: E016
priority: P1
tags: [viewer, items, tooling]
created: 2026-07-05
updated: 2026-07-16
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

- [x] Viewer shows the catalog of every registered game/template with icons and
      no per-game JavaScript data model.
- [x] After T0386 it reads bounded focused Snapshot queries from Items Lua; no
      consumer parses `items.json` or recalculates game math.
- [ ] Typed blocks, levels, overrides, cost lists, diagnostics, release state,
      source links, grid/chart, semantic diff, and what-if render from generated
      schema/snapshot metadata.
- [ ] What-if data is never a build input and can only become a reviewed Lua
      patch or `Edit with agent` request.
- [ ] Developer UI and AI share semantic literal/table/curve/override edits with
      expected hash, diff, undo, conflict refusal, and full validation. The UI
      invokes T0366 operations directly and stores/replays only their returned
      inverse patches; it owns no second writer.
- [ ] Items-only empty/error/loading behavior, path boundaries, icon resolution,
      and Windows browser tests remain green through cutover.

## Open questions

- This task owns exact chart/grid/diff/what-if interaction and payload budgets.
- T0386 owns the one-shot JSON/schema/parser deletion and backend switch.
- T0366 and this task own restricted semantic editing; arbitrary Lua write-back stays
  out of scope.

## Log

- 2026-07-14: Marked the already accepted phase-1 Viewer criterion complete and
  removed the stale T0364 prerequisite; remaining criteria start at Snapshot
  cutover and Workbench behavior.

- 2026-07-15: Confirmed ownership boundary with T0366: this task owns Developer
  UI invocation plus session undo storage/replay over T0366 inverse patches.

- 2026-07-14: Moved from E009 to E016 and absorbed T0367. One Workbench card now
  owns grid/chart/diff/what-if and shared semantic editing instead of a separate
  specification task.

- 2026-07-08: Phase 1 accepted. Its current contract is maintained in
  `ai_studio/assets/items_viewer/README.md`; implementation history remains in
  git. Follow-up icon work linked compiled pack previews and retained read-only
  failure/path boundaries.
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
- 2026-07-16: Started after T0386 completed with green Ubuntu/Windows CI. First slice will expose one bounded Workbench detail payload from the existing semantic CLI rather than adding browser-side catalog logic.
- 2026-07-16: Slice 1 added focused registered-catalog item and selected-series HTTP reads by composing the existing inspect, schema, source, dependencies, and chart CLI operations. Snapshot objects stay unchanged, charts remain lazy, and no browser or Node evaluator/model was added. RED/green Viewer tests pass 22/22.
- 2026-07-16: Slice 2 replaced the repetitive card grid with the accepted table-first master/detail surface. Selected detail renders typed core/capability blocks, raw level rows, normalized costs, provenance, release state, diagnostics, dependencies, checked source location, and lazy selected-series charts directly from Slice 1 payloads. Viewer tests pass 23/23; Playwright at 320 and 1440 px shows zero page overflow, accessible named item buttons with preserved focus, one level row for tmpl.sword, all focused API requests 200, and zero console warnings/errors.
- 2026-07-16: Slice 3 added the single Developer UI semantic edit bridge over T0366. The adapter accepts only bounded level-set, override-set, or curve-set patches with expected source hash; preview omits --apply, apply uses the CLI lock/validation/atomic replacement, conflicts return 409, and undo replays the exact returned inverse patch. RED/green tests prove preview writes nothing, apply changes Lua, stale hash refuses, and inverse restores byte-identical source; Viewer suite passes 25/25.
- 2026-07-16: Slice 4 completed the browser interaction over that bridge. The Workbench derives editable operations and fields from Snapshot provenance, keeps what-if state visibly ephemeral, requires a reviewed preview before Apply, and stores only returned inverse patches in its session Undo stack. Unsupported generated/source shapes refuse and route to checked source or agent editing. Playwright verified the real template refusal state and a routed editable fixture at 320/1440 px with focused Apply, exact source/semantic diffs, no overflow, and zero console warnings/errors; Viewer suite passes 28/28.

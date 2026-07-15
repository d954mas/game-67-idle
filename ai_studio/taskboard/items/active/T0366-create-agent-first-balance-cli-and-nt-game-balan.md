---
id: T0366
title: Create focused Items CLI and agent skill routing
status: doing
project: P001
epic: E016
priority: P1
tags: [items, balance, cli, skill, agent]
created: 2026-07-10
updated: 2026-07-15
---

## What

Expose the shared evaluator/snapshot through compact semantic operations so an
agent and developer UI use one inspect/edit/validate/build contract without
loading the whole package. Keep `nt-game-items` as the single router for this
Items-owned scope.

## Done when

- [ ] Commands cover schema, list, inspect item/level/dependencies, validate,
      build, source navigation, and focused reports without a second evaluator.
- [ ] Shared writes cover recognized scalar literals, explicit table cells,
      built-in curve parameters, and overrides with expected hash, canonical
      format, atomic same-file batch, source+semantic diff, inverse patch, and
      conflict refusal; UI session undo stores returned inverse patches only.
- [ ] Commands distinguish `level-set` for literal rows, `override-set` for
      computed cells, and `curve-set` for built-in parameters; safe max-level append/truncate has
      release/migration gates, `diff`, and formula/unsupported-source refusal;
      inserting a level that renumbers shipped rows is not a normal operation.
- [ ] Structural row writes require a source-preserving CST or proven canonical
      region writer; spans alone permit only demonstrated safe token replacement.
- [ ] V1 refuses a batch spanning multiple Lua files. A future multi-file writer
      requires an explicit journal/recovery protocol; one canonical owner does
      not imply one physical source file.
- [ ] Every command accepts explicit `--project-root` or workspace-qualified
      game context, never guesses a current game from cwd, and returns bounded stable
      JSON, source locations, fingerprints, and non-zero failure.
- [ ] V1 `--affected` may run a full evaluation internally but returns only the
      requested error/dependency neighborhood.
- [ ] Developer UI and AI invoke the same semantic ops. Formula/helper/control
      flow edits route to source or `Edit with agent`; no arbitrary Lua writer
      is introduced.
- [ ] `nt-game-items` remains a short router and does not duplicate Lua API,
      schema, numeric policy, or game data; no speculative Balance router is
      created by this task.
- [ ] Benchmarks report command count, latency, file reads, stdout/context bytes,
      and diagnostic quality for representative item edits.

## Open questions

- Exact Lua CST/source-preserving implementation is selected by a writer spike;
  restricted semantic writing is required target behavior, not optional scope.

## Log

- 2026-07-14: Removed the speculative separate Balance router; the current
  vertical and CLI are Items-owned.

- 2026-07-10: Re-scoped after Items became canonical Lua rather than an external
  JSON source joined by a separate Balance CLI.
- 2026-07-10: Human/AI UX re-review required one typed patch schema, explicit
  literal/override/curve operations, atomic batch paste, and inverse patches.
- 2026-07-10: Red-team review limited v1 atomicity to one source file; modular
  Lua makes multi-file rollback a separate durability feature.
- 2026-07-10: Absorbed the durable explicit-context requirement from superseded E015 task T0360.
- 2026-07-15: Started after T0365 local implementation reached review. First slice will inventory and consolidate existing evaluator/Snapshot operations behind one explicit-context Items CLI before adding any write surface.
- 2026-07-15: Slice 1: added one explicit-project read-only semantic CLI over the existing isolated Lua evaluator and Snapshot APIs. Bounded list/inspect/dependencies/source/schema/validate routes return stable JSON, refuse cwd game inference and manifest escape, and keep nt-game-items a short router while legacy JSON remains labeled until T0386. Exposed items-core 1.10.0 with synchronized receipt/template pins. Review ACCEPT; CLI 4/4, items_ops 29/29, Python contracts 7/7, feature contracts pass, benchmark/native targets build, studio verify --changed passed harness/features/template-release, and diff check clean.

---
id: T0366
title: Create focused Items and Balance CLI plus agent skill routing
status: backlog
project: P001
epic: E016
priority: P1
tags: [items, balance, cli, skill, agent]
created: 2026-07-10
updated: 2026-07-14
---

## What

Expose the shared evaluator/snapshot through compact semantic operations so an
agent and developer UI use one inspect/edit/validate/build contract without
loading the whole package. Keep `nt-game-items` for the Items domain and create
a short Balance router only for non-Items balance workflows.

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
- [ ] Skills remain short routers and do not duplicate Lua API, schema, numeric
      policy, or game data.
- [ ] Benchmarks report command count, latency, file reads, stdout/context bytes,
      and diagnostic quality for representative item edits.

## Open questions

- Exact Lua CST/source-preserving implementation is selected by a writer spike;
  restricted semantic writing is required target behavior, not optional scope.

## Log

- 2026-07-10: Re-scoped after Items became canonical Lua rather than an external
  JSON source joined by a separate Balance CLI.
- 2026-07-10: Human/AI UX re-review required one typed patch schema, explicit
  literal/override/curve operations, atomic batch paste, and inverse patches.
- 2026-07-10: Red-team review limited v1 atomicity to one source file; modular
  Lua makes multi-file rollback a separate durability feature.
- 2026-07-10: Absorbed the durable explicit-context requirement from superseded E015 task T0360.

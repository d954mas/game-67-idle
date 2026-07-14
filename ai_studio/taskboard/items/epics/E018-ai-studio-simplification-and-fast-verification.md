---
id: E018
title: AI Studio simplification and fast verification
status: done
project: P001
priority: P2
tags: [refactor, simplification, verification]
created: 2026-07-13
updated: 2026-07-13
---

## Goal

Remove post-refactor duplication and ceremony while preserving the quality,
privacy, ownership, and release guarantees established by E015. Make the
normal Windows agent loop small, predictable, and measurably fast.

## In scope

- Coarse owner verification domains, focused changed checks, explicit release
  proof, and continued measurement-driven test acceleration.
- A module-level Architecture Map rather than an authored filesystem/test
  inventory.
- A structured Taskboard Quality decision that actually blocks invalid
  closeout.
- A small useful Taskboard context profiler without benchmark ceremony.
- Deletion of obsolete Canvas/history/backlog/migration surfaces.
- One canonical Windows agent launcher and honest shared-config ownership.
- Delegation only when an independent bounded packet earns its coordination
  cost.

## Out of scope

- Weakening Quality, provenance, privacy, game-owned verification, or the
  Windows/Linux CI release proof.
- Formal L0-L4 labels, per-test Architecture Map nodes, or another generic test
  framework.
- Removing or demoting settings and resource-panel feature packs.
- Routine WSL usage, a Studio UI redesign, or unrelated game/feature work.

## Accepted decisions

- Keep three public verification intents only: changed work, one owner domain,
  and full release proof. Do not introduce L0-L4, `--fast`, or a separate
  generic `--integration` taxonomy.
- Continue measured acceleration after the first 39.2% improvement. Optimize
  the slow suites themselves before adding runners, caches, or frameworks.
- Keep Quality mandatory and make its evidence/outcome contract stronger;
  tests prove behavior, while Quality judges whether the result is acceptable.
- Remove tests from the authored Architecture Map. The map describes modules,
  ownership, public entry points, canonical contracts/stores, stable external
  boundaries, and verification domains; subtree discovery covers internals.
- Keep the useful privacy-safe Taskboard context-size profiler, but delete its
  benchmark, latency, telemetry, and duplicate-output ceremony.
- Delete obsolete Canvas/history/research documents, the empty asset backlog,
  and migration-only structural guards after preserving any unique live
  invariant in the owning short contract.
- Keep exactly one documented detached Windows agent launcher. WSL is not a
  routine local route; Linux remains release/CI proof.
- Keep settings and resource-panel as expandable feature packs. They are not
  cleanup targets and do not need separate top-level verification suites.
- Keep configuration physically small and return interpretation to real domain
  owners instead of creating a configuration framework.
- Replace broad mandatory delegation with cost-benefit routing while retaining
  delegation for independent research, parallel work, and adversarial review.
- Scale review to risk: none for mechanical/docs/moves, one reviewer for normal
  logic, two for security/concurrency/release; repeat only after high-risk
  findings or a contract change.

## Rejected or superseded approaches

- A formal hierarchy of test levels and a hand-maintained per-test registry.
- A new generic test framework, a new map generator, or infrastructure whose
  measured saving does not justify its maintenance cost.
- Treating free-form Quality text, test success alone, or an unexplained skip
  as sufficient closeout evidence.
- Routine WSL calls from the Windows agent path.
- Removing or demoting settings/resource-panel, or preserving dead surfaces
  through compatibility paths.
- Delegating coherent local work only because it spans multiple files.
- Creating disposable agent identities or tight wait polling when an existing
  role and event-driven wait can do the same work.

## Execution order

1. T0412 establishes the verification shape and performance budget; T0417 may
   proceed independently.
2. T0413, T0414, T0415, and T0418 simplify map, Taskboard, config, and agent
   policy boundaries.
3. T0416 removes obsolete surfaces after their replacement contracts exist.

## Log

- 2026-07-13: Created after the post-E015 architecture and performance review.
  The lead accepted the simplification directions and requested continued test
  acceleration as durable plan work.
- 2026-07-13: Current measured Windows baseline improved from 186.4 seconds to
  113.3 seconds with all 28 suites passing; T0412 owns the remaining work.
- 2026-07-13: Final closeout: all seven tasks are archived done. Real-worktree Windows full passed ten owner domains in 49.3s versus the 186.4s baseline, a 73.6% wall-time reduction (3.78x faster). Ubuntu full remains the mandatory pre-merge CI gate; no WSL substitute was used.

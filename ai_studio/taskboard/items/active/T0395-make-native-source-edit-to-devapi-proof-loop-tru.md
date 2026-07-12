---
id: T0395
title: Make native source-edit-to-DevAPI-proof loop trustworthy and benchmark it
status: doing
project: P001
epic: E015
priority: P1
tags: [runtime-automation, template, performance, devapi]
created: 2026-07-10
updated: 2026-07-12
---

## What

Make the reference template's native agent iteration loop trustworthy before
optimizing it. A normal source-edit iteration must build from current inputs,
launch a fresh DevAPI-enabled binary, wait for semantic readiness, run one
universal proof command, and emit compact phase timings.

Runtime Automation owns universal build/launch/readiness/proof orchestration;
the reference template owns deterministic benchmark fixtures. T0357 remains
the owner of Python/CMake toolchain and build-phase optimization, while T0353
owns the Studio facade and Windows/Linux CI duration.

## Done when

- [ ] A regression test proves that an existing executable cannot be treated as
      current after a relevant source or generated input changes.
- [ ] Normal iteration invokes the canonical incremental build contract every
      time and lets the build system decide whether the build is a true no-op;
      no custom timestamp or dependency scanner is introduced.
- [ ] The prepared native configuration explicitly enables DevAPI, launches a
      fresh process, and defines readiness as a successful universal DevAPI
      request rather than process creation or window appearance.
- [ ] One deterministic semantic proof demonstrates that the launched binary
      contains the current edit, so screenshots and runtime evidence cannot
      silently come from a stale binary.
- [ ] `--reuse` is documented and reported as attach-only interaction with an
      already-running unchanged game; it makes no build-freshness or code-
      iteration claim.
- [ ] Human output stays concise and a stable JSON result separates validation/
      codegen, configure when invoked, compile, link, launch-to-DevAPI-ready,
      semantic proof, and total wall time, plus tool-call count and output bytes.
- [ ] Benchmarks keep cold, true no-op, one leaf-C edit, and one content/codegen/
      pack edit as separate scenarios; warm results report median and p90 from
      at least 20 measured runs, while 3-5 cold runs are reported separately.
- [ ] Results record commit/worktree state, OS, CPU, compiler, generator, CMake/
      build-tool versions, rebuilt targets/files, and the exact command path.
- [ ] The generated VS Code build path and direct incremental build path are
      compared only to locate phase overhead; any CMake/toolchain optimization
      is routed to T0357 rather than implemented a second time here.
- [ ] Windows has a reproducible checked-in local baseline; Linux CI proves the
      functional contract. CI timing remains advisory and no blocking budget is
      introduced until the lead accepts a stable baseline.
- [ ] Focused Runtime Automation tests, reference-template build/proof smoke,
      and Taskboard validation pass.

## Open questions

None.
## Log

- 2026-07-10: `iterate.py` advertised build-if-stale but only checked whether
  the executable existed, allowing runtime evidence from a stale binary.
- 2026-07-10: Lead chose a separate small native source-edit-to-DevAPI-proof
  task instead of expanding T0357 or creating a generic performance framework.
- 2026-07-10: Planning only. No Runtime Automation or template implementation
  was started by this decision.
- 2026-07-10: Execution dependency fixed as T0357 -> T0353 -> T0395.
- 2026-07-10: T0357 -> T0353 -> T0395 and web/game/engine ownership boundaries are fixed; no remaining planning question.
- 2026-07-12: Execution checkpoint after T0353 dual-platform CI closeout a758b5860. Inspect current Runtime Automation and reference-template paths first; preserve E017/game WIP and keep external/neotolis-engine read-only.

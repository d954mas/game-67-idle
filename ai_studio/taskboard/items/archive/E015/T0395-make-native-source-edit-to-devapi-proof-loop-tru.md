---
id: T0395
title: Make native source-edit-to-DevAPI-proof loop trustworthy
status: done
project: P001
epic: E015
priority: P1
tags: [runtime-automation, template, devapi, freshness]
created: 2026-07-10
updated: 2026-07-13
---

## What

Make the reference template's native agent iteration loop trustworthy. A normal
source-edit iteration must build from current inputs,
launch a fresh DevAPI-enabled binary, wait for semantic readiness, run one
universal proof command, and emit compact phase timings.

Runtime Automation owns universal build/launch/readiness/proof orchestration;
the reference template owns deterministic semantic proof fixtures. T0357 remains
the owner of Python/CMake toolchain performance and its checked-in Windows
baseline at `ai_studio/dev_environment/benchmarks/template.windows.json`, while
T0353 owns the Studio facade and Windows/Linux CI duration. T0395 consumes those
surfaces and adds no performance budget or statistical benchmark.

## Done when

- [x] A regression test proves that an existing executable cannot be treated as
      current after a relevant source or generated input changes.
- [x] Normal iteration invokes the canonical incremental build contract every
      time and lets the build system decide whether the build is a true no-op;
      no custom timestamp or dependency scanner is introduced.
- [x] The prepared native configuration explicitly enables DevAPI, launches a
      fresh process, and defines readiness as a successful universal DevAPI
      request rather than process creation or window appearance.
- [x] One deterministic semantic proof demonstrates that the launched binary
      contains the current edit, so screenshots and runtime evidence cannot
      silently come from a stale binary.
- [x] `--reuse` is documented and reported as attach-only interaction with an
      already-running unchanged game; it makes no build-freshness or code-
      iteration claim.
- [x] Human output stays concise and a stable JSON result separates validation,
      configure when invoked, metadata, codegen, compile, link,
      launch-to-DevAPI-ready, semantic proof, capture consistency, and total
      wall time, plus tool-call count and output bytes.
- [x] One real functional smoke each proves true no-op, one leaf-C edit, and one
      content/codegen/pack edit as separate scenarios. T0395 does not repeat the
      statistical cold/warm/no-op benchmark already checked in by T0357.
- [x] Results record commit/worktree state, OS, CPU, compiler, generator, CMake/
      build-tool versions, rebuilt targets/files, and the exact command path.
- [x] Existing T0357 Windows baseline evidence is linked rather than repeated;
      any CMake/toolchain or VS Code-path optimization is routed to T0357.
- [x] Linux CI proves the functional contract. CI timing remains advisory and
      no blocking performance budget is introduced by this task.
- [x] Focused Runtime Automation tests, reference-template build/proof smoke,
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
- 2026-07-13: Lead removed the redundant 3-5 cold plus 20x no-op/leaf-C/content
  statistical benchmark. T0357 already records that performance baseline;
  T0395 now keeps one real functional smoke per incremental scenario and owns
  freshness correctness only.
- 2026-07-12: Execution checkpoint after T0353 dual-platform CI closeout a758b5860. Inspect current Runtime Automation and reference-template paths first; preserve E017/game WIP and keep external/neotolis-engine read-only.
- 2026-07-13: Implemented the v2 trustworthy loop: unconditional canonical
  incremental build, DevAPI-only template semantic proof for one leaf-C fixture
  and one generated schema fixture, positive-PID plus endpoints readiness,
  exact proof and artifact hashes, race-only Git worktree guards, truthful
  configure/metadata/codegen/compile/link/capture phase reporting, and
  `freshnessClaim:false` on every failure and attach-only result.
- 2026-07-13: Windows functional evidence on the restored real worktree passed
  once per required scenario. True no-op: compile 354.287 ms and zero rebuilt
  files. Leaf-C edit: compile 594.762 ms, exact live value
  `template-leaf-c-cycle2`, and only `iteration_proof_devapi.c.obj` plus
  `game.exe` rebuilt. Schema edit: compile 769.342 ms, exact live value
  `Template ready cycle2`, with generated game-state outputs, dependents, and
  `game.exe` rebuilt. All three runs retained a positive PID, stable scoped
  worktree/engine guards, matching artifact hashes, and
  `freshnessClaim:true`; temporary fixture edits were restored and the baseline
  binary rebuilt afterward.
- 2026-07-13: Focused verification is green: Runtime Automation 58/58,
  reference-template DevAPI 13/13, Studio 13/13, Architecture Map strict with
  zero issues, and Taskboard validation with zero problems. T0357's checked-in
  Windows benchmark remains the sole performance baseline. Task remains doing
  pending independent re-review and the Linux functional CI command.
- 2026-07-13: Two independent closure reviews converged with 0 HIGH, 0 MEDIUM,
  and 0 actionable findings. Reviewers reran Runtime Automation 58/58,
  Studio+CI 15/15, and template smoke 8/8; the stale-executable regression now
  uses a real pre-existing executable and two iterations around real C/schema
  fixture edits. Linux installs `xvfb` and runs the exact functional proof under
  `xvfb-run -a`.
- 2026-07-13: Quality: QTECH_001=review; evidence: focused behavior tests and
  Windows no-op/leaf/schema runtime proofs pass; real Linux CI proof remains the
  final evidence gate before task closure.
- 2026-07-13: GitHub Actions 29227311178 passed the complete Windows job but
  failed Ubuntu only at `semanticProof`: `endpoints` succeeded, then the next
  immediate proof request hit a `TimeoutError`. The real log exposed that
  `connect_existing` used its multi-second connect window but returned a client
  with a hard-coded 1.0-second request timeout.
- 2026-07-13: TDD repair: a new regression first failed with expected 5.0s vs
  actual 1.0s; `connect_existing` now keeps the standard 5.0-second DevAPI
  request timeout independently from its retry/connect window. Focused GREEN:
  DevAPI client 17/17 and iterate 22/22. Linux CI rerun remains pending.
- 2026-07-13: Independent timeout review found the first repair also applied
  5.0s to TCP connect and could overrun 0.1/0.5s preflight callers. A refined
  regression failed with expected constructor timeout 1.0s vs actual 5.0s;
  established clients now connect with the bounded 1.0s attempt and then set
  the socket request timeout to 5.0s. Full Runtime Automation GREEN: 59/59.
- 2026-07-13: Final architecture review found one LOW cleanup edge for an
  invalid negative request timeout. The regression failed because the newly
  created client was not closed; timeout-configuration errors now close before
  re-raising. DevAPI client 18/18 and full Runtime Automation 60/60 pass.
- 2026-07-13: GitHub Actions run 29230677691 passed the exact commit
  `2dfb46aa6` on Ubuntu and Windows. Ubuntu completed in 7m39s with the blocking
  full suite in 6m24s, including the real `xvfb-run` build/launch/readiness/
  semantic-proof contract; Windows completed in 10m32s with its blocking full
  suite in 8m34s. Evidence:
  https://github.com/d954mas/game-67-idle/actions/runs/29230677691
- 2026-07-13: Quality: QTECH_001=pass; evidence: Runtime Automation 60/60,
  template and Studio focused suites green, two independent final reviews with
  0 actionable findings, and GitHub Actions 29230677691 green on Ubuntu and
  Windows with the Linux functional proof.

---
id: T0349
title: Enforce truthful Taskboard closure and structured quality evidence
status: done
project: P001
epic: E015
priority: P0
tags: [taskboard, quality, context]
created: 2026-07-10
updated: 2026-07-11
---

## What

Make future Taskboard closure truthful and machine-checkable through the one
existing mutation path. A new transition to `done` must prove completion or
an explicit waiver and must record one explicit quality applicability decision
in `## Log`; no second quality store or automatic rule selector is added.

Existing-state reconciliation, ID allocation, and context profiling remain
owned by T0375, T0373, and T0374.

## Done when

- [x] New `done` requires every acceptance criterion checked, or a canonical
      explicit waiver with a non-empty reason and completion evidence.
- [x] New `done` also requires either one or more
      `Quality: Q...=<outcome>; evidence: ...` entries or
      `Quality: not-applicable; reason: ...` in the task log.
- [x] Missing closure or quality decisions fail early with concise human and
      machine-readable errors; non-`done` transitions are unaffected.
- [x] `taskboard set` may accept structured closure/quality inputs only by
      appending the canonical log representation through the existing update
      path; it does not create parallel fields or storage.
- [x] The guard checks explicit presence and shape only. It does not infer
      applicable rules, execute checks, or decide release sufficiency.
- [x] Existing archived/done tasks remain readable and are grandfathered
      without migration; legacy gaps may be reported as warnings.
- [x] Focused store, CLI, and API tests cover pass, not-applicable, waiver,
      malformed, and missing-decision cases.
- [x] Taskboard and quality docs define the contract once and cross-link it.
- [x] No WIP limit, SLA, mandatory reviewer, or automatic closure is added.

## Open questions

None.
## Log

- 2026-07-10: Split after final transcript audit so lifecycle enforcement
  remains independent from ID/context/reconciliation work.
- 2026-07-10: Absorbed the accepted presence-only quality closeout contract
  from duplicate T0394. T0394 is closed as superseded planning, not as an
  implemented quality gate.
- 2026-07-10: Blocking/review/unverified outcome semantics remain with owning quality workflows; this presence gate does not create a global release policy.
- 2026-07-11: Checkpoint: Wave 1 gate closed at 3ad425da7. Starting schema-presence closure enforcement through the existing Taskboard mutation path only; T0374 context profiling, T0393 audio, E016, WIP limits, SLA, reviewer policy, and automatic quality selection remain out of scope.
- 2026-07-11: TDD evidence: initial focused closeout tests produced 5 expected store/CLI/API failures. Three review/fix cycles added RED regressions for create-done bypass, malformed or hidden criteria/evidence, fence/comment/section parsing, duplicate Quality ids, and profiler drift before each GREEN fix.
- 2026-07-11: Implementation: `updateDoc` is the single guarded non-done to done path; new done tasks cannot be created directly. CLI structured inputs append canonical dated Log records before the same update, API returns the same machine problem, and existing done history is grandfathered.
- 2026-07-11: Verification: Taskboard suite 55/55, Quality profiler 3/3, Taskboard validation 0 problems, Core Harness doc-reference check 10/10, strict Architecture Map 352 mapped / 784 scanned with 0 issues, and scoped diff check pass.
- 2026-07-11: Review convergence: cycle 1 fixed 2 HIGH and 2 MEDIUM plus process gaps; cycle 2 fixed 1 HIGH and 3 MEDIUM parser findings; cycle 3 fixed 2 MEDIUM and profiler alignment. Targeted final architecture and process rechecks report 0 HIGH and 0 actionable MEDIUM/LOW.
- 2026-07-11: Quality: QTECH_001=pass; evidence: RED/GREEN store, CLI, API, Markdown scanner, creation guard, canonical evidence, profiler alignment, full module regression, validator, and independent review proof.
- 2026-07-11: Closed after three review/fix cycles converged at 0 HIGH and 0 actionable; all acceptance criteria and QTECH_001 evidence are present.

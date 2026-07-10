---
id: T0375
title: Reconcile stale Taskboard statuses and cancel obsolete migration
status: done
project: P001
epic: E015
priority: P1
tags: [taskboard, reconciliation]
created: 2026-07-10
updated: 2026-07-10
---

## What

Perform the one-time evidence-based status cleanup and resolve direct conflicts
between old migration work and the decisions from this audit.

## Done when

- [x] Reconcile `T0242`, `T0327`, `T0328`, `T0337`, `T0341`, `T0346`, and other
      review/doing cards from checked criteria, logs, and current repository
      evidence; do not repeat completed implementation.
- [x] Cancel/supersede `T0347` as a migration task because `T0356` now owns
      deletion of `rb-dark-rpg`; preserve its useful inventory/evidence in logs.
- [x] The reported 34-review/6-doing snapshot is remeasured before mutation and
      every status transition has a reason/evidence line.
- [x] No bulk auto-close, WIP policy, SLA, or mandatory reviewer is introduced.
- [x] Taskboard validate passes after reconciliation and no active cards retain
      contradictory ownership.

## Open questions

## Log

- 2026-07-10: Final transcript audit found the direct `T0347` migration versus
  `T0356` deletion conflict; this task resolves it before refactor execution.
- 2026-07-10: Selected as Wave 0 and moved to todo. This is the first execution task after plan approval; no reconciliation implementation was performed during planning.
- 2026-07-11: Checkpoint: execution started from measured baseline tasks=16 idea / 52 backlog / 2 todo / 6 doing / 34 review / 6 done. Scope is one-time evidence reconciliation only; preserve T0393 WIP, introduce no lifecycle policy, and do not repeat product implementation.
- 2026-07-11: Reconciliation evidence: 20 completed/superseded/routed cards were moved through the canonical CLI into archive, six pre-existing `status: done` files were normalized from active storage into archive, T0316 moved from stale doing to dependency-blocked E016 backlog, and 21 review cards with real unchecked decisions or live acceptance remained in review. Every transition has a card-local reason.
- 2026-07-11: Ownership evidence: T0347 now has checked supersession criteria, preserved 46-file Taskboard/Canvas inventory, no open migration question, and explicit transfer of deletion/reference/archive-marker scope to T0356. T0393 remains backlog and its audio WIP was not modified by this task.
- 2026-07-11: Verification: `node ai_studio/taskboard/cli.mjs validate --json` pass; `node --test ai_studio/taskboard/tests/taskboard.test.mjs` 30/30 pass; `git diff --check -- ai_studio/taskboard` pass; active done-card count 0.
- 2026-07-11: Independent diff review cycles: cycle 1 found 0 HIGH / 2 actionable evidence issues; cycle 2 found 0 HIGH / 1 residual contradiction; cycle 3 converged independently on 0 HIGH / 0 actionable for architecture/correctness/ownership and tests/process/performance/context cost.
- 2026-07-11: Quality: QTECH_001=pass; evidence: canonical lifecycle transitions, store validation, 30/30 Taskboard tests, scoped diff check, and two clean independent final reviews.
- 2026-07-10: Closed after three review-fix-verify cycles converged at 0 HIGH / 0 actionable; Taskboard validate, 30/30 tests, scoped diff check, and active-done storage check pass.

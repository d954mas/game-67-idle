---
id: T0375
title: Reconcile stale Taskboard statuses and cancel obsolete migration
status: backlog
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

- [ ] Reconcile `T0242`, `T0327`, `T0328`, `T0337`, `T0341`, `T0346`, and other
      review/doing cards from checked criteria, logs, and current repository
      evidence; do not repeat completed implementation.
- [ ] Cancel/supersede `T0347` as a migration task because `T0356` now owns
      deletion of `rb-dark-rpg`; preserve its useful inventory/evidence in logs.
- [ ] The reported 34-review/6-doing snapshot is remeasured before mutation and
      every status transition has a reason/evidence line.
- [ ] No bulk auto-close, WIP policy, SLA, or mandatory reviewer is introduced.
- [ ] Taskboard validate passes after reconciliation and no active cards retain
      contradictory ownership.

## Open questions

## Log

- 2026-07-10: Final transcript audit found the direct `T0347` migration versus
  `T0356` deletion conflict; this task resolves it before refactor execution.

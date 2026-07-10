---
id: T0349
title: Enforce truthful Taskboard closure and structured quality evidence
status: backlog
project: P001
epic: E015
priority: P0
tags: [taskboard, quality, context]
created: 2026-07-10
updated: 2026-07-10
---

## What

Make future Taskboard closure truthful and machine-checkable without creating a
second mutation path. Existing-state reconciliation, ID allocation, and context
profiling are owned by `T0375`, `T0373`, and `T0374` respectively.

## Done when

- [ ] New `done` requires checked criteria or an explicit waived reason plus
      evidence/applicability; legacy gaps are reported as warnings.
- [ ] `taskboard set` accepts structured quality/evidence inputs without a
      second task mutation path.
- [ ] Legacy cards without the new structure remain readable and produce
      actionable warnings rather than being silently rewritten.
- [ ] No WIP limit, SLA, mandatory reviewer, or automatic closure is introduced.

## Open questions

## Log

- 2026-07-10: Split after final transcript audit so lifecycle enforcement cannot
  be marked done while ID/context/reconciliation work remains unfinished.

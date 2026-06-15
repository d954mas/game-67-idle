---
id: T0047
title: Retire over-built validation planner machinery
status: backlog
epic: E003
priority: P1
tags: [validation, speed, tooling, subtraction]
created: 2026-06-15
updated: 2026-06-15
---

## What

Eight consecutive tasks (T0035-T0042) built a "validation planner" that infers
scoped checks from touched files, plus per-tier re-runs - `validation_run.mjs`
was the single most-recorded tool on 06-15 (86 entries). This is machinery built
instead of using a simple fixed default. Audit what the planner actually saves
versus its cost, then collapse it to the minimum (or remove it) so selecting
checks is cheap and obvious, not a subsystem. Example of the subtraction the
lead asked for.

## Done when

- [ ] Documented audit: which planner-inferred scopes are actually used and what they save versus running a fixed quick set.
- [ ] Planner reduced to a single simple scope selection (or removed) - no preflight/scoped/final re-run cascade for normal work.
- [ ] Dead/unused planner tools removed (mark superseded tasks, delete code) without breaking quick/full validate (T0043).
- [ ] Relevant tests + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

- Full removal vs minimal keep - decide from the usage audit.

## Log

- 2026-06-15: Created from full pipeline review. T0035-T0042 added validation-planner machinery; usage data shows heavy re-run churn, not game progress.

---
id: T0027
title: Add orchestrator subagent protocol
status: done
epic: E001
priority: P0
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Add the first small, enforceable orchestration protocol for using subagents in
this repo. This should make the main agent act as lead/integrator while
delegating bounded, independent packets to researchers, workers, reviewers, or
verifiers.

### Scope

- Add a hot route for when subagents are required, useful, or harmful.
- Define the packet schema in a cold reference.
- Define hot-file ownership, integration rules, and budget decisions.
- Add a short repo invariant without bloating hot docs.

### Out Of Scope

- No new game work.
- No mechanical validator yet; collect one iteration of practice first.
- No large workflow engine or taskboard schema change.

## Done when

- [x] `docs/ai-pipeline/agent-workflow.md` routes to the subagent protocol
      without exceeding hot-doc budgets.
- [x] `docs/ai-pipeline/subagent-protocol.md` contains the detailed
      orchestrator/subagent protocol.
- [x] `AGENTS.md` states the short repo-level subagent boundary.
- [x] The protocol names packet schema, ownership, forbidden hot files,
      integration, verification expectations, and budget decision rules.
- [x] Validation passes for taskboard/docs/context budget/export.

## Open questions

- None for this first increment.

## Log

- 2026-06-21: Started after lead feedback that the agent was doing too much
  solo work, causing context bloat and missed parallelism.
- 2026-06-21: First draft exceeded normal hot-doc pressure, so the protocol was
  decomposed: short route in `agent-workflow.md`, details in
  `subagent-protocol.md`, and a compact invariant in `AGENTS.md`.
- 2026-06-21: Used subagents for the first iteration: one read-only explorer
  reviewed placement/budget risks, then an independent reviewer found and fixed
  a P1 hot-file ownership loophole plus a missing budget/correctness rule.
- 2026-06-21: Evidence passed:
  `node tools/context_budget.mjs`,
  `node tools/context_budget.mjs --review`,
  `node tools/doc_reference_check.mjs`,
  `node tools/taskboard/cli.mjs validate`,
  `node --test tools/bootstrap/export_base.test.mjs`,
  `node tools/ai.mjs validate --review`, and
  `AI_PIPELINE_PYTHON=<bundled-python> node tools/ai.mjs validate --full`.

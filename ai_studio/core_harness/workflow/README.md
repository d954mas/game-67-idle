# Core Harness Workflow

Workflow defines the lead-agent work loop: load scoped context, do the work,
validate it, and close out with evidence.

## Use

Load this file when changing context policy, work-loop behavior, delegation, or
hot agent docs.

For substantial work, add only task-specific context:

- `node ai_studio/taskboard/cli.mjs context --json`
- one task/evidence file when durable tracking is useful
- one matching skill

Prefer scoped search and compact output over whole-file dumps. Use archives,
logs, generated artifacts, and broad design only when task-linked or requested.

## Work Loop

1. Interpret the request into one working scope.
2. Use an existing task when durable tracking is useful. Only the lead creates
   new Taskboard items, except through lead-invoked `/to-spec` or `/to-tickets`
   (see [hard invariants](../../../AGENTS.md#hard-invariants)).
3. Read only files needed for the selected scope.
4. Make the smallest coherent change.
5. Run the narrowest validation that proves the change.
6. Record evidence in the task log and final response when project state changes.

Before substantial implementation, make the outcome, scope and proof clear in
the task or discussion. Run focused checks as the work changes; use
`studio.mjs verify --changed` for changed shared Studio owners. The full Studio
gate runs in CI on master and before Studio publication or release work. A
routine task can close with narrower evidence. Run CI once per SHA.
After the same unexplained failure twice, diagnose the cause instead of retrying
the same command again.

## Checkpoint And Handoff

Checkpoint when the task changes hands, pauses, or needs a fresh context.
`profiling/status.mjs --complete` reports advisory session limits; the
[profiling guide](../profiling/README.md) owns their thresholds.

Keep handoffs compact: link canonical decisions and state, name what was
proven, and give the next actionable step.

## Quality Feedback

Use [quality rules](../../quality/README.md) when a changed player-facing,
design, asset, runtime or release claim needs evidence. The matching rule's
`Use When` decides; there is no check at every iteration point.

Do not create project-local quality rules. Tools and templates should capture
review evidence and acceptance notes, not define or link quality rule IDs.

## Expansion Boundaries

Before expanding active game feature/content work, check current project state.
Do not expand when lead rejection is unresolved, references are explicitly not
ready, the runtime is becoming monolithic without an architecture task, or the
user said the prototype/game is stopped, done, or only a test.

Record any override as explicit lead acceptance, not as an agent decision.

## Enforcement Boundary

Mechanical gates are CI `verify --full` on master, `doc_reference_check`,
`agent_surfaces sync --check`, and the Taskboard quality gate for closing work
in `store.mjs`. Everything else is advisory; the lead is the backstop.

Pin the engine submodule only to reviewed commits from the engine's main branch
(`master`).

## Task Log

When a durable task exists, use its `## Log` for decisions, evidence, handoffs
and remaining work. Update it with
`node ai_studio/taskboard/cli.mjs set <id> --log "..."`; record integrated
delegated results once, without copying worker transcripts. Taskboard owns the
item lifecycle.

## Delegation

Keep coherent work with the lead. Use orchestration only for an independent
bounded packet whose latency, context, or review benefit exceeds packet writing,
context transfer, and reintegration cost. Detailed packet, review-budget,
approval, reuse, and waiting rules live in `orchestration/README.md`.

## Files

- `README.md`: canonical short workflow contract.
- `orchestration/README.md`: delegation rule for broad read-heavy work.

## Validation

Workflow docs are checked by:

```powershell
node ai_studio/core_harness/validation/doc_reference_check.mjs
node ai_studio/architecture_map/validate_map.mjs
```

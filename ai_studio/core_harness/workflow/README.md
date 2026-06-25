# Core Harness Workflow

Workflow defines how the lead agent turns a user request into scoped context,
work, validation, optional delegation, and closeout.

This module belongs to `ai_studio/core_harness/` because it is harness behavior,
not task state and not domain implementation.

## Use

Load this file when changing context policy, work-loop behavior, multi-agent use,
or hot agent docs.

Default substantial-work context:

- `AGENTS.md`
- `node ai_studio/taskboard/cli.mjs context`
- one task/evidence file when durable tracking is useful
- one matching skill

Prefer scoped search and compact output over whole-file dumps. Use archives,
logs, generated artifacts, and broad design only when task-linked or requested.

## Work Loop

1. Interpret the request into one working scope.
2. Select or create a task only when durable tracking is useful.
3. Read only files needed for the selected scope.
4. Make the smallest coherent change.
5. Run the narrowest validation that proves the change.
6. Record evidence in the task/status/final response when project state changes.

## Delegation

Single-agent is the default. Use subagents only for independent research,
disjoint edits, generation, review, or verification.

The lead owns hot docs, task/status changes, integration, final validation,
commits, and the final report. Acceptance gates the delivered work, not the fact
that delegation happened.

Before broad reading, use `../orchestration/README.md` to decide whether to
split the work early. Do not load all context yourself first and only then
delegate.

## Files

- `README.md`: canonical short workflow contract.

## Validation

Workflow docs are checked by:

```powershell
node ai_studio/core_harness/validation/doc_reference_check.mjs
node ai_studio/architecture_map/validate_map.mjs
```

Do not add mandatory proof-of-delegation gates here.

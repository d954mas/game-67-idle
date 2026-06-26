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
2. Select or create a task only when durable tracking is useful.
3. Before broad reading, decide what to delegate with `orchestration/README.md`.
4. Read only files needed for the selected scope.
5. Make the smallest coherent change.
6. Run the narrowest validation that proves the change.
7. Record evidence in the task/final response when project state changes.

## Delegation

Delegate non-trivial work early. Stay single-agent only for quick edits, narrow
questions, or tightly coupled changes where splitting would add more overhead
than it saves.

## Files

- `README.md`: canonical short workflow contract.
- `orchestration/README.md`: delegation rule for broad read-heavy work.

## Validation

Workflow docs are checked by:

```powershell
node ai_studio/core_harness/validation/doc_reference_check.mjs
node ai_studio/architecture_map/validate_map.mjs
```

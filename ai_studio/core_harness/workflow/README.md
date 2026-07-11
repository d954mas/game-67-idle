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
7. Record evidence in the task log and final response when project state changes.

## Quality Feedback

Quality checks are iterative feedback, not only final acceptance gates.

During substantial work, pause at natural iteration points and ask whether
quality feedback would reduce risk or rework:

- before expensive implementation;
- after the first draft;
- after visible, runtime, design, asset, or document changes;
- before expanding scope;
- before closeout.

Use quality feedback when the task changes player-facing output, game design,
assets, GDD, runtime behavior, or release-facing state. If useful, choose only
relevant rules from `ai_studio/quality/README.md`.

Do not create project-local quality rules. Tools and templates should capture
review evidence and acceptance notes, not define or link quality rule IDs.

## Expansion Boundaries

Before expanding active game feature/content work, check current project state.
Do not expand when lead rejection is unresolved, references are explicitly not
ready, the runtime is becoming monolithic without an architecture task, or the
user said the prototype/game is stopped, done, or only a test.

Record any override as explicit lead acceptance, not as an agent decision.

## Enforcement Boundary

Material rules are classified in
[`enforcement_contract.json`](enforcement_contract.json) as `host-enforced`,
`repository-validator-enforced`, or `process convention`. Logs and quality
outcomes report observed evidence; they do not imply a repository model router
or turn an advisory process instruction into technical enforcement.

## Task Log

When a durable task is selected, `## Log` is the handoff record for the lead and
orchestrator. Use it for status changes, validation evidence, review handoff,
acceptance, rejection, pauses, and integrated delegated work.

Use `node ai_studio/taskboard/cli.mjs set <id> --log "..."` for short
checkpoints during work. Keep the log useful, not ceremonial: record what changed,
what was proven, what was delegated, what was accepted or rejected, and what
remains unclear.

Entry shape:

```text
- YYYY-MM-DD: Changed/validated/accepted <scope>. Evidence: <command or result>. Open: <optional next issue>.
```

Use `Observed:` only for host or validator evidence that actually ran. Use
`Advisory:` for review conclusions and process conventions; neither task logs
nor Quality outcomes turn advisory instructions into enforcement.

Delegation entries should show the worker task and the integrated outcome:

```text
- YYYY-MM-DD: Delegated: <worker task>. Return: <expected result shape>.
- YYYY-MM-DD: Worker result: <short finding/result>. Integrated: <accepted change or decision>.
```

## Delegation

Delegate non-trivial work early. Stay single-agent only for quick edits, narrow
questions, or tightly coupled changes where splitting would add more overhead
than it saves.

## Files

- `README.md`: canonical short workflow contract.
- `orchestration/README.md`: delegation rule for broad read-heavy work.
- `enforcement_contract.json`: enforcement labels and evidence routes.

## Validation

Workflow docs are checked by:

```powershell
node ai_studio/core_harness/validation/doc_reference_check.mjs
node ai_studio/core_harness/validation/enforcement_check.mjs
node ai_studio/architecture_map/validate_map.mjs
```

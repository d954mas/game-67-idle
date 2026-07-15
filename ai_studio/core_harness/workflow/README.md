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
3. Read only files needed for the selected scope.
4. Make the smallest coherent change.
5. Run the narrowest validation that proves the change.
6. Record evidence in the task log and final response when project state changes.

For substantial implementation, state four short fields before changing code:

```text
Goal: concrete outcome
Scope: allowed modules
Done: observable completion condition
Proof: commands or evidence that establish Done
```

Use the validation ladder once per meaningful state change: focused proof during
RED/GREEN, `studio.mjs verify --changed` after the coherent change, and one
`verify --full` before publishing or closing the work. Run CI once per SHA.
After the same unexplained failure twice, diagnose the cause instead of retrying
the same command again.

## Checkpoint And Handoff

Checkpoint when the task finishes, scope or domain changes, two or three
independent commits accumulate, an external pause exceeds ten minutes, or the
lead leaves a long-running work packet. For long sessions, checkpoint after four
hours; start a fresh session after six hours or when current model context reaches
70%. A 300-tool-call session also needs a checkpoint. These are advisory
boundaries: continuing is allowed when the reason is explicit.

`profiling/status.mjs --complete` reports `continue`,
`checkpoint-recommended`, or `new-session-recommended` from the evidence the
Codex transcript exposes.

Keep handoff content compact and point to canonical state instead of copying it:

```text
Task:
Accepted decisions:
Current SHA and dirty state:
Changed:
Proven:
Remaining:
Next command:
Do not repeat:
```

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

Mechanical gates are CI `verify --full` on master, `doc_reference_check`,
`agent_surfaces sync --check`, and the Taskboard quality gate for closing work
in `store.mjs`. Everything else is advisory; the lead is the backstop.

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

Delegation entries should show the worker task and the integrated outcome:

```text
- YYYY-MM-DD: Delegated: <worker task>. Return: <expected result shape>.
- YYYY-MM-DD: Worker result: <short finding/result>. Integrated: <accepted change or decision>.
```

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

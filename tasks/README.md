# Task Store

Source of truth for current work. Detailed protocol:
`tasks/guides/task-store-reference.md`.

- Active: `tasks/active/`; epics: `tasks/epics/`.
- Review/closed history: `tasks/archive/`.
- Live index: `tasks/STATUS.md`.

Archives are history; load only for linked evidence, regression debug, review
cleanup, or user request.

## Commands

- Orient: `node tools/taskboard/cli.mjs summary` or `context`.
- Inspect: `list`, `list --review`, `show T0001`.
- Change: `new task --title "..." --epic E001 --priority P1`, `set T0001 --status doing`.
- Validate: `node tools/taskboard/cli.mjs validate`.
- Orchestrate: `node tools/ai.mjs orchestration-check --current --json` previews
  a subagent packet (advisory). For delegated work, follow the compact operator
  path in `docs/ai-pipeline/agent-workflow.md`. Acceptance gates the work
  product, not the delegation.
- Board when requested: `node tools/taskboard/server.mjs`.
- New game: `node tools/game_context/new_prototype.mjs --game-id <id> --title "<name>" --brief "<one sentence>"`.

## Live Status

`tasks/STATUS.md` is a short index, not a log. Every claim points to a task,
doc, command, or evidence. Keep history in task logs; update status only when
goal, blockers, gates, validation, evidence, or priorities change.

## Minimal Context

For substantial work: `AGENTS.md` -> `node tools/taskboard/cli.mjs context` ->
needed task/evidence files -> one matching skill.

Search current scope only. Avoid archives, review queues, P3 ideas, broad design,
and build artifacts unless linked.

## Done And Validation

A task is done only when `## Done when` is checked and `## Log` proves it. Use
the guide for lifecycle, scope intake, evidence, checkpoints, and manual format.

Validation by change type: `docs/ai-pipeline/quality-validation.md`.
Repeated strict/product failures: `node tools/product_gate/repeated_failure_guard.mjs`.

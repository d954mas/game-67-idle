# Task Store

Source of truth for current work.

- Active: `tasks/active/`
- Review/closed: `tasks/archive/`
- Epics: `tasks/epics/`
- Live index: `tasks/STATUS.md`

Archived tasks are history; load only for linked evidence, regression debugging,
review cleanup, or explicit user request.

## Commands

```powershell
node tools/taskboard/cli.mjs summary
node tools/taskboard/cli.mjs context
node tools/taskboard/cli.mjs list
node tools/taskboard/cli.mjs list --review
node tools/taskboard/cli.mjs show T0001
node tools/taskboard/cli.mjs new task --title "..." --epic E001 --priority P1
node tools/taskboard/cli.mjs set T0001 --status doing
node tools/taskboard/cli.mjs validate
```

Board/server only when requested: `node tools/taskboard/server.mjs`.

```powershell
node tools/game_context/new_prototype.mjs --game-id <id> --title "<name>" --brief "<one sentence>"
```

## Live Status

`tasks/STATUS.md` is a short operational index: goal, blockers, debt, gate,
validation, evidence, next priorities. It is not a log.

Every claim points to a task, doc, command, or evidence path. Keep history in
task logs. Update only when goal/gate/blockers, validation, evidence, or
priorities change.

## Minimal Context

For substantial work:

1. Read `AGENTS.md`.
2. Run `node tools/taskboard/cli.mjs context`.
3. Read only task/evidence files needed for the decision.
4. Load one matching skill.

Default searches stay current/scoped: avoid completed tasks, review queues, old
logs, P3 ideas, unrelated epics, broad design docs, and build artifacts.

## Detailed Protocol

Load `tasks/guides/task-store-reference.md` only when changing/auditing task
lifecycle, scope intake, backlog, evidence, checkpoints, or manual format.

## Done And Validation

A task is done only when `## Done when` boxes are checked and `## Log` proves
them. If validation is slow, unavailable, or environment-broken, record it.

Smallest reliable gates:

- docs/status/process: `node tools/taskboard/cli.mjs validate`
- skill/process: `node tools/skills_eval.mjs`
- product gate/tooling: `node --test tools/product_gate/test.mjs`
- taskboard/tooling: `node --test tools/taskboard/test.mjs`
- reusable pipeline: `node tools/pipeline_validate.mjs`
- portable/export/runtime: `node tools/pipeline_validate.mjs --full`
- visual/playable: native scenario plus screenshot/video/gate evidence

Repeated strict/product failures:

```powershell
node tools/product_gate/repeated_failure_guard.mjs
```

# Task Store

Single source of truth for current work and live project status.

- Current tasks: `tasks/active/`
- Archived done/dropped tasks: `tasks/archive/<epic-id>/` or
  `tasks/archive/unassigned/`
- Epics: `tasks/epics/`
- Live status index: `tasks/STATUS.md`

Archived tasks are history; load only for evidence, review, regression
debugging, or linked current work.

## Commands

```powershell
node tools/taskboard/cli.mjs summary
node tools/taskboard/cli.mjs context
node tools/taskboard/cli.mjs list
node tools/taskboard/cli.mjs show T0001
node tools/taskboard/cli.mjs new task --title "..." --epic E001 --priority P1
node tools/taskboard/cli.mjs set T0001 --status doing
node tools/taskboard/cli.mjs validate
```

Hidden lanes: add `--ideas`, `--review`, or `--archive` to `list`.

Board, only for board work or user requests:

```powershell
node tools/taskboard/server.mjs
```

Fresh game kickoff:

```powershell
node tools/game_context/new_prototype.mjs --game-id <id> --title "<name>" --brief "<one sentence>"
```

## Live Status

`tasks/STATUS.md` is a short operational index, not a log. It answers current
goal, blockers, debt, gate, validation, last evidence, and next priorities.

Every concrete claim should point to a task, doc, command, or evidence path.
Keep history in task logs, not status; budgets enforce this.

Update `STATUS.md` when goal, gate, blockers, validation, evidence, or
priorities change.

## Minimal Context

For substantial work:

1. Read `AGENTS.md`.
2. Run `node tools/taskboard/cli.mjs context`.
3. Read only task/evidence files needed for the decision.
4. Load one matching skill.

Do not load completed tasks, review queues, old logs, P3 ideas, unrelated
epics, broad design docs, or build artifacts by default.

Search current context before history; scope searches before repo-wide search.

## Detailed Protocol

Load `tasks/guides/task-store-reference.md` when changing/auditing lifecycle,
scope intake, backlog refinement, evidence, checkpoints, or hand-written format.

## Done And Evidence

A task is done only when `## Done when` boxes are checked and `## Log` proves
them. Detailed lifecycle rules live in the guide.

Smallest reliable validation by change type:

- docs/status/process: `node tools/taskboard/cli.mjs validate`
- skill/process: `node tools/skills_eval.mjs`
- product gate/tooling: `node --test tools/product_gate/test.mjs`
- taskboard/tooling: `node --test tools/taskboard/test.mjs`
- reusable pipeline: `node tools/pipeline_validate.mjs`
- portable/export/runtime: `node tools/pipeline_validate.mjs --full`
- visual/playable game change: native scenario plus screenshot/video/gate
  evidence

Repeated strict/product failures are validated by:

```powershell
node tools/product_gate/repeated_failure_guard.mjs
```

If validation is slow, unavailable, or fails for unrelated environment reasons,
record it explicitly. Do not silently mark the task done.

# Task Store

Single source of truth for current work items and live project status.

- Current tasks: `tasks/active/`
- Archived done/dropped tasks: `tasks/archive/<epic-id>/` or
  `tasks/archive/unassigned/`
- Epics: `tasks/epics/`
- Live status index: `tasks/STATUS.md`

Archived tasks are history. Load them only for evidence, review, regression
debugging, or a current link.

## Commands

```powershell
node tools/taskboard/cli.mjs summary
node tools/taskboard/cli.mjs context
node tools/taskboard/cli.mjs list
node tools/taskboard/cli.mjs list --ideas
node tools/taskboard/cli.mjs list --review
node tools/taskboard/cli.mjs list --archive
node tools/taskboard/cli.mjs show T0001
node tools/taskboard/cli.mjs new task --title "..." --epic E001 --priority P1
node tools/taskboard/cli.mjs set T0001 --status doing
node tools/taskboard/cli.mjs validate
```

Board:

```powershell
node tools/taskboard/server.mjs
```

Open `http://127.0.0.1:8070/` only when the task is explicitly about the board
or the user asks for it.

Fresh game kickoff:

```powershell
node tools/game_context/new_prototype.mjs --game-id <id> --title "<name>" --brief "<one sentence>"
```

## Live Status

`tasks/STATUS.md` is a short operational index, not a log. It should answer:

- current goal;
- blocking work;
- non-blocking debt;
- current gate;
- required validation;
- last known good evidence;
- next priorities.

Every concrete claim should point to a task, design doc, command, or evidence
path. Keep `STATUS.md` compact; `tools/context_budget.mjs` enforces the hot-doc
cap and taskboard validation keeps it below the hard store limit.

Update `STATUS.md` when current goal, gate, blockers, required validation, last
known good evidence, or next priorities change. Put detailed history in task
logs, not status.

## Minimal Context

For long-running work, resumes, planning, multi-agent work, and code/doc edits:

1. Read `AGENTS.md`.
2. Run `node tools/taskboard/cli.mjs context`.
3. Read only the task/evidence files directly needed for the decision.
4. Load one matching skill.

Do not load completed tasks, review queues, old logs, P3 ideas, unrelated
epics, broad design docs, or build artifacts by default. Prefer `summary` and
`context` over full `STATUS.md` reads unless editing or auditing status.

Search current context before history. Scope code/design searches to the likely
area (`src/`, `tools/`, `.codex/skills/`, active project folder) before using
repo-wide search.

## Detailed Protocol

Load `tasks/guides/task-store-reference.md` when changing or auditing task
lifecycle, scope intake, backlog refinement, evidence rules, checkpoint shape,
or hand-written task format. Do not load it for normal orientation.

## Done And Evidence

A task is done only when `## Done when` boxes are checked and `## Log` contains
the evidence that proves them.

Smallest reliable validation by change type:

- docs/status/process: `node tools/taskboard/cli.mjs validate`
- skill/process: `node tools/skills_eval.mjs`
- product gate/tooling: `node --test tools/product_gate/test.mjs`
- taskboard/tooling: `node --test tools/taskboard/test.mjs`
- reusable pipeline: `node tools/pipeline_validate.mjs`
- portable/export/runtime: `node tools/pipeline_validate.mjs --full`
- visual/playable game change: native scenario plus screenshot/video/product
  gate evidence

Repeated strict/product gate failures are validated by:

```powershell
node tools/product_gate/repeated_failure_guard.mjs
```

If validation is too slow, unavailable, or fails for an unrelated environment
reason, record that explicitly. Do not silently mark the task done.

---
name: task-manager
description: Use when capturing, refining, decomposing, planning, prioritizing, or reporting project work items. Triggers include new feature ideas, "add a task", "what should we do next", breaking a large request into smaller tasks, grouping work into epics, backlog grooming, marking work done, deferred "later" work that must not be lost, and any request to look at or update the task board.
---

# Task Manager

Keep all project work captured, refined, and planned in the markdown task
store. The user is the lead: high-level direction and feedback only; agents
question, research, refine, decompose, and execute.

Store and live-status rules: `tasks/README.md`.
Live status index: `tasks/STATUS.md`.
Fast status: `node tools/taskboard/cli.mjs summary`.
Compact current-context digest: `node tools/taskboard/cli.mjs context`.
CLI: `node tools/taskboard/cli.mjs <summary|list|context|show|new|set|validate>`.
`list` shows current work only; use `list --review` only for review cleanup.
User-facing board: `node tools/taskboard/server.mjs` -> `http://127.0.0.1:8070/`.

## Workflow

1. Follow `tasks/README.md` for the minimal current-context protocol, status
   updates, task format, and state transitions. Start quick orientation with
   `node tools/taskboard/cli.mjs summary`; use
   `node tools/taskboard/cli.mjs context` for long work. Read full
   `tasks/STATUS.md` only when changing it or auditing a specific linked claim.
2. Capture anything the user wants that will not be done right now as a task.
   Losing a stated idea is a failure; when in doubt, capture as `status: idea`.
3. Refine before implementing: an `idea` task needs answered user questions,
   researched context, and checkable `## Done when` before it moves to `backlog`.
4. Decompose large requests into an epic plus tasks each completable in one
   focused session; scope boundaries live in the epic's in/out-of-scope.
5. Track honestly: `doing` on start, `review` when awaiting feedback, `done`
   only with ticked criteria and an evidence line in `## Log`.

Treat `review` as a separate acceptance/cleanup queue, not default current
work. Do not inspect the review queue during normal game implementation unless
the user asks to review/close old tasks or the current decision depends on a
specific review item.

When the user says a prototype/game was only a test run or should stop, close
the active game context instead of leaving it in `doing`/`review`: set related
tasks and epics to `dropped` with a log entry, let the tooling archive task
files, rewrite `tasks/STATUS.md` to "no active game concept selected", and keep
only reusable lessons in pipeline docs/skills. Never delete task files to hide
closed work.

## Refinement bar

A `backlog` task must answer: what visible change proves it done, what is out
of scope, and which docs/data/skills it touches. Otherwise it stays `idea`
with `## Open questions` for the user.

When the user asks "where are we" or "what next", follow the reporting rules in
`tasks/README.md`.

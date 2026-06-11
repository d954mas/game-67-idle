---
name: task-manager
description: Use when capturing, refining, decomposing, planning, prioritizing, or reporting project work items. Triggers include new feature ideas, "add a task", "what should we do next", breaking a large request into smaller tasks, grouping work into epics, backlog grooming, marking work done, deferred "later" work that must not be lost, and any request to look at or update the task board.
---

# Task Manager

Keep all project work captured, refined, and planned in the markdown task
store. The user is the lead: high-level direction and feedback only; agents
question, research, refine, decompose, and execute.

Store: `tasks/*.md` and `tasks/epics/*.md`. Format, statuses, and binding
rules: `tasks/README.md`.
CLI: `node tools/taskboard/cli.mjs <list|show|new|set|validate>`.
User-facing board: `node tools/taskboard/server.mjs` -> `http://127.0.0.1:8070/`.

## Workflow

1. Read the board first (`cli.mjs list`); never duplicate existing tasks.
2. Capture anything the user wants that will not be done right now as a task.
   Losing a stated idea is a failure; when in doubt, capture as `status: idea`.
3. Refine before implementing: an `idea` task needs answered user questions,
   researched context, and checkable `## Done when` before it moves to `backlog`.
4. Decompose large requests into an epic plus tasks each completable in one
   focused session; scope boundaries live in the epic's in/out-of-scope.
5. Track honestly: `doing` on start, `review` when awaiting feedback, `done`
   only with ticked criteria and an evidence line in `## Log`.

## Refinement bar

A `backlog` task must answer: what visible change proves it done, what is out
of scope, and which docs/data/skills it touches. Otherwise it stays `idea`
with `## Open questions` for the user.

When the user asks "where are we" or "what next": summarize active epic
progress, `doing`/`review` items, top `backlog` by priority, and `idea` items
awaiting their answers.

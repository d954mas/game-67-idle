---
name: task-manager
description: Use when capturing, refining, decomposing, planning, prioritizing, or reporting project work items. Triggers include new feature ideas, "add a task", "what should we do next", breaking a large request into smaller tasks, grouping work into epics, backlog grooming, marking work done, deferred "later" work that must not be lost, and any request to look at or update the task board.
---

# Task Manager

Keep all project work captured, refined, and planned in the markdown task
store. The user is the lead: high-level direction and feedback only; agents
question, research, refine, decompose, and execute.

## Load Only What Applies

- `references/task-store-protocol.md`: task/status source of truth, CLI usage,
  state transitions, review queue handling, prototype closeout, refinement bar,
  and reporting rules.

## Fast Commands

- Summary: `node tools/taskboard/cli.mjs summary`.
- Current-context digest: `node tools/taskboard/cli.mjs context`.
- CLI: `node tools/taskboard/cli.mjs <summary|list|context|show|new|set|validate>`.
- User-facing board: `node tools/taskboard/server.mjs` ->
  `http://127.0.0.1:8070/`.

## Workflow

1. Start quick orientation with `cli.mjs summary`; use `cli.mjs context` for
   long work. Read full `tasks/STATUS.md` only when changing it or auditing a
   specific linked claim.
2. Follow `tasks/README.md` and load the task-store protocol when changing
   tasks, status, epics, review items, or reporting shape.
3. Capture anything the user wants that will not be done right now as a task.
   Losing a stated idea is a failure; when in doubt, capture as `status: idea`.
4. Refine before implementing; a `backlog` task needs checkable `## Done when`
   and enough context to execute without guessing.
5. Track honestly: `doing` on start, `review` when awaiting feedback, `done`
   only with ticked criteria and an evidence line in `## Log`.

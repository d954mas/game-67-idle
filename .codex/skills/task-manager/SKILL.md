---
name: task-manager
description: Use when capturing, refining, decomposing, planning, prioritizing, or reporting project work items. Triggers include new feature ideas, "add a task", "what should we do next", breaking a large request into smaller tasks, grouping work into epics, backlog grooming, marking work done, deferred "later" work that must not be lost, and any request to look at or update the task board.
---

# Task Manager

Keep project work captured, refined, and planned in the markdown task store.

## Load Only What Applies

- `references/task-store-protocol.md`: task/status source of truth, CLI usage,
  state transitions, review queue, prototype closeout, refinement, reporting rules.

## Fast Commands

- Summary: `node tools/taskboard/cli.mjs summary`.
- Current-context digest: `node tools/taskboard/cli.mjs context`.
- CLI: `node tools/taskboard/cli.mjs <summary|list|context|show|new|set|validate>`.
- Board: `node tools/taskboard/server.mjs` -> `http://127.0.0.1:8070/`.

## Workflow

1. Start with `cli.mjs summary`; use `cli.mjs context` for long work. Read full
   `tasks/STATUS.md` only when changing it or auditing a linked claim.
2. Follow `tasks/README.md` and load the task-store protocol when changing
   tasks, status, epics, review items, or reporting shape.
3. Capture anything the user wants that will not be done right now as a task.
   Losing a stated idea is a failure; when in doubt, capture as `status: idea`.
4. Refine before implementing; `backlog` needs checkable `## Done when`.
5. Track honestly: `doing` on start, `review` when awaiting feedback, `done`
   only with ticked criteria and an evidence line in `## Log`.

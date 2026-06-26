---
name: nt-taskboard-manager
description: "Use this skill when routing project task work through AI Studio Taskboard: capturing, refining, decomposing, planning, prioritizing, or reporting work items. Triggers include new feature ideas, add a task, what should we do next, breaking a large request into smaller tasks, grouping work into epics, backlog grooming, marking work done, deferred later work that must not be lost, and any request to look at or update the task board."
---

# NT Taskboard Manager

Use this as a thin router to `ai_studio/taskboard/`. Do not duplicate the
Taskboard data contract here.

## Sources

- Product/API entry: `ai_studio/taskboard/README.md`.
- Detailed task-store contract: `ai_studio/taskboard/task-store-reference.md`.
- Current-game routing: `GAME_PROJECT.md`.

## Commands

- Summary: `node ai_studio/taskboard/cli.mjs summary --json`.
- Current work: `node ai_studio/taskboard/cli.mjs context --json`.
- List rows: `node ai_studio/taskboard/cli.mjs list --json`.
- Read one item: `node ai_studio/taskboard/cli.mjs show T0001 --json`.
- Mutate through CLI: `new`, `set`, then `validate --json`.
- Board: `node ai_studio/studio_shell/server.mjs`, then open `/taskboard/`.

## Workflow

1. Start with `summary --json`; use `context --json` only for longer work.
2. Load `task-store-reference.md` only when changing task fields, statuses,
   epics, lifecycle rules, or markdown format.
3. Capture deferred work as `status: idea`; use `backlog` only after scope and
   checkable done criteria are clear.
4. Mark `done` only when `## Done when` is checked and `## Log` has evidence.

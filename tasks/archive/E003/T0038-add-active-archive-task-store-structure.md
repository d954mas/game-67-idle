---
id: T0038
title: Add active/archive task store structure
status: done
epic: E003
priority: P1
tags: [ai-pipeline, taskboard, archive]
created: 2026-06-12
updated: 2026-06-12
---

## What

Make the task store faster for long-running agents by separating current task
context from historical task evidence. Current work should live in
`tasks/active/`; completed or dropped work should live in `tasks/archive/` and
stay accessible by task id.

## Done when

- [x] Taskboard reads current tasks from `tasks/active/` by default.
- [x] Done/dropped tasks live in `tasks/archive/<epic-id-or-unassigned>/`.
- [x] `show`/`set` can still find archived task IDs.
- [x] CLI can include archived history explicitly.
- [x] Exported new projects get the same folder structure.
- [x] Existing task files are migrated without deleting history.
- [x] Taskboard validation and tests pass.

## Open questions

None.

## Log

- 2026-06-12: Added `tasks/active/` and `tasks/archive/` support to taskboard,
  migrated existing task files, verified archived task lookup with `show T0037`,
  validated current store, ran `node --test tools/taskboard/test.mjs`, and
  verified `tools/bootstrap/export_base.mjs` creates a valid active/archive task
  store in a new exported project.
- 2026-06-12: Closed active/archive task-store migration; validation, tests, archived lookup, and exported-project validation passed.

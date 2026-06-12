---
id: T0039
title: Add checkpoint and handoff discipline
status: done
epic: E003
priority: P1
tags: [ai-pipeline, context, handoff]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add a reusable checkpoint/handoff rule so long-running agents do not leave
important project state only in chat. The rule should define when to update task
logs, when to update `STATUS.md`, and what must be included before pausing or
ending a substantial session.

## Done when

- [x] `tasks/README.md` defines checkpoint/handoff behavior.
- [x] The rule keeps `STATUS.md` short and task logs detailed.
- [x] `tasks/STATUS.md` mentions the new discipline without duplicating it.
- [x] Taskboard validation passes.

## Open questions

None.

## Log

- 2026-06-12: Added `Checkpoints And Handoff` to `tasks/README.md`, added a
  short `STATUS.md` index note, and validated with
  `node tools/taskboard/cli.mjs validate`.
- 2026-06-12: Closed checkpoint/handoff rule; tasks/README.md and STATUS.md updated; taskboard validate passed.

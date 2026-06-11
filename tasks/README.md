# Task Store

Single source of truth for project work items. One task = one markdown file
here; one epic = one markdown file in `epics/`. The visual board and the CLI
both read and write these files; agents may also edit them directly.

## Run the board

```powershell
node tools/taskboard/server.mjs
```

Open `http://127.0.0.1:8070/`.

## CLI

```powershell
node tools/taskboard/cli.mjs list
node tools/taskboard/cli.mjs new task --title "..." --epic E001 --priority P1
node tools/taskboard/cli.mjs set T0001 --status doing
node tools/taskboard/cli.mjs show T0001
node tools/taskboard/cli.mjs validate
```

## Format

```markdown
---
id: T0001
title: Camp rest action
status: backlog
epic: E001
priority: P2
tags: [state, camp]
created: 2026-06-11
updated: 2026-06-11
---

## What

## Done when

- [ ] checkable acceptance criteria

## Open questions

## Log
```

## Rules

- Task statuses: `idea -> backlog -> todo -> doing -> review -> done`, plus
  `dropped`. Epic statuses: `idea -> active -> done`, plus `dropped`.
- `idea` means raw and unexamined; do not implement from an `idea` task.
  Refine it first: ask questions, research, split, then move to `backlog`.
- Never delete a task file to "remove" work; set `status: dropped` and note
  why in `## Log`. History is part of the value.
- Prefer `cli.mjs new` over hand-creating files so IDs never collide.
- Keep `## Done when` checkable; when finishing a task, tick the boxes and add
  a one-line evidence note (command, screenshot path, scenario name) in `## Log`.
- Priorities: `P0` blocking now, `P1` this iteration, `P2` normal, `P3` someday.
- Run `validate` after bulk edits.

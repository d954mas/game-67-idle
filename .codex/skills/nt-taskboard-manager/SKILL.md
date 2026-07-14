---
name: nt-taskboard-manager
description: "Use this skill when routing project, epic, or task work through AI Studio Taskboard: creating or updating projects for games/templates/AI Studio work, capturing/refining/decomposing/planning/prioritizing/reporting work items, backlog grooming, marking work done, and any request to inspect or update the task board."
---

# NT Taskboard Manager

Use this as a thin router to `ai_studio/taskboard/`. Do not duplicate the
Taskboard data contract here.

## Sources

- Product/API entry: `ai_studio/taskboard/README.md`.
- Detailed task-store contract: `ai_studio/taskboard/task-store-reference.md`.
- Current-game routing: `games/<game-id>/`.

## Commands

- Summary: `node ai_studio/taskboard/cli.mjs summary --json`.
- Current work: `node ai_studio/taskboard/cli.mjs context --json`.
- List rows: `node ai_studio/taskboard/cli.mjs list --json`.
- Read one item: `node ai_studio/taskboard/cli.mjs show <P###|E###|T####> --json`.
- Help: `node ai_studio/taskboard/cli.mjs help`.
- Create project: `node ai_studio/taskboard/cli.mjs new project --title "..." --kind <ai-studio|game|template|tooling|research|other> --target <path>`.
- Create epic: `node ai_studio/taskboard/cli.mjs new epic --title "..." --project P###`.
- Create task: `node ai_studio/taskboard/cli.mjs new task --title "..." --project P### --epic E### --priority P1`.
- Update item: `node ai_studio/taskboard/cli.mjs set <id> --status <status> --log "..." --json`.
- Validate after mutations: `node ai_studio/taskboard/cli.mjs validate --json`.
- Board: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Open`, then open `/taskboard/`.
  Run it with host-process permission outside the managed sandbox; never use WSL.

## Workflow

1. Start with `summary --json`; use `context --json` only for longer work.
2. Use projects as top-level owners, epics as grouped slices, and tasks as
   actionable cards.
3. Load `task-store-reference.md` only when changing project/task fields,
   statuses, epics, lifecycle rules, or markdown format.
4. Capture deferred work as `status: idea`; use `backlog` only after scope and
   checkable done criteria are clear.
5. Mark `done` only when `## Done when` is checked and `## Log` has evidence.

## Boundary

Keep this skill as a router. Taskboard code, schemas, markdown templates,
browser UI, API payloads, and validation belong in `ai_studio/taskboard/`.

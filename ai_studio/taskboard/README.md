# Task Store

Source of truth for current work. Detailed protocol:
`ai_studio/taskboard/task-store-reference.md`.

- Active: `tasks/active/`; epics: `tasks/epics/`.
- Review/closed history: `tasks/archive/`.

Archives are history; load only for linked evidence, regression debug, review
cleanup, or user request.

## Product Surface

The browser product is a compact task board: active work columns only
(`backlog`, `todo`, `doing`, `review`) plus search and epic filter. Epics are
metadata, not a second navigation layer. Enable "all statuses" only when
reviewing raw ideas or archive state.

Run it through Studio Shell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

Open `/taskboard/`.

## Agent API

Prefer JSON when an agent needs task state:

- Orient: `node ai_studio/taskboard/cli.mjs summary --json`.
- Current work: `node ai_studio/taskboard/cli.mjs context --json`.
- List rows: `node ai_studio/taskboard/cli.mjs list --json`.
- Read one file: `node ai_studio/taskboard/cli.mjs show T0001 --json`.
- Change: `node ai_studio/taskboard/cli.mjs new task --title "..." --epic E001 --priority P1`,
  `node ai_studio/taskboard/cli.mjs set T0001 --status doing --log "..." --json`.
- Validate store shape: `node ai_studio/taskboard/cli.mjs validate --json`.

The browser board uses `ai_studio/taskboard/api.mjs` for `/api/board`,
`/api/tasks`, `/api/epics`, and `/api/agent/context`. Studio Shell only mounts
the API and serves the surface.

## Minimal Context

For substantial work: `node ai_studio/taskboard/cli.mjs context --json` ->
needed task/evidence files -> `GAME_PROJECT.md` only for game-specific work ->
one matching skill.

Search current scope only. Avoid archives, P3 ideas, broad design, and build
artifacts unless linked.

## Done And Validation

A task is done only when `## Done when` is checked and `## Log` explains the
evidence. Use the guide for lifecycle, scope intake, evidence, checkpoints, and
manual format.

Validation by change type: `ai_studio/quality/README.md`.
Repeated quality failures should be visible in task logs and summarized with
`node ai_studio/quality/profile.mjs`.

Taskboard validation checks task-store structure only.

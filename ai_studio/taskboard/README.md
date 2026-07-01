# Task Store

Source of truth for current work. Detailed protocol:
`ai_studio/taskboard/task-store-reference.md`.

- Projects: `ai_studio/taskboard/items/projects/`; epics:
  `ai_studio/taskboard/items/epics/`; active tasks:
  `ai_studio/taskboard/items/active/`.
- Review/closed history: `ai_studio/taskboard/items/archive/`.

Archives are history; load only for linked evidence, regression debug, review
cleanup, or user request.

## Product Surface

The browser product is a compact task board: active work columns only
(`backlog`, `todo`, `doing`, `review`) plus search, project filter, and epic
filter. Projects are the top-level work owner; epics group slices inside a
project; tasks are the actionable cards. Enable "all statuses" only when
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
- Help: `node ai_studio/taskboard/cli.mjs help`.
- Change: `node ai_studio/taskboard/cli.mjs new project --title "..." --kind game --target games/<id>`,
  `node ai_studio/taskboard/cli.mjs new epic --title "..." --project P001`,
  `node ai_studio/taskboard/cli.mjs new task --title "..." --project P001 --epic E001 --priority P1`,
  `node ai_studio/taskboard/cli.mjs set T0001 --status doing --log "..." --json`.
- Validate store shape: `node ai_studio/taskboard/cli.mjs validate --json`.

The browser board uses `ai_studio/taskboard/api.mjs` for `/api/board`,
`/api/projects`, `/api/epics`, `/api/tasks`, item reads like
`/api/tasks/T0001`, and `/api/agent/context`. List endpoints return compact
metadata; item reads return the markdown body for editing. Studio Shell only
mounts the API and serves the surface.

Reusable integrations should treat Taskboard as a feature boundary:

- Use `ai_studio/taskboard/cli.mjs` for agent/human commands.
- Use `ai_studio/taskboard/lib.mjs` only for direct store operations:
  `findRoot`, list/find, project ensure/create, item create/update, payload
  builders, and validate.
- Do not import the HTTP adapter for payload construction.
- Do not treat private store details as public API.

Internal module ownership is intentionally small:

- `store.mjs`: markdown store, frontmatter parse/serialize, path layout,
  constants, templates, create/update/list/find, archive movement, stable JSON
  payloads, and validation.
- `cli.mjs`: public human/agent command surface.
- `api.mjs`: HTTP adapter only; Studio Shell mounts it but does not own the
  Taskboard domain.

There is no root-level task-store fallback. The canonical store lives only under
`ai_studio/taskboard/items/`.

Search is currently an in-browser metadata filter over the loaded board payload:
id, title, tags, project, epic, kind, and target. Markdown files remain the
source of truth because they are readable, git-reviewable, and easy for agents
to edit. SQLite is appropriate for Taskboard only as a generated index/cache if
we later need large full-text search, 10k+ live items, cross-history analytics,
or low-latency queries across archived logs. It should not replace markdown as
the canonical store without that pressure.

## Minimal Context

For substantial work: `node ai_studio/taskboard/cli.mjs context --json` ->
needed task/evidence files -> `games/<game-id>/` only for game-specific work ->
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

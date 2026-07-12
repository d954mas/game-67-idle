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
project; tasks are the actionable cards. Use the Statuses filter when reviewing
raw ideas or closed state.

Run it through Studio Shell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

Open `/taskboard/`.

## Agent API

Prefer JSON when an agent needs task state:

- Orient: `node ai_studio/taskboard/cli.mjs summary --json`.
- Current work: `node ai_studio/taskboard/cli.mjs context --json` (at most five
  body-free task summaries by default). Use an explicit `--tasks-limit <n>`
  only for a scoped routing decision.
- List rows: `node ai_studio/taskboard/cli.mjs list --json`.
- Read one file: `node ai_studio/taskboard/cli.mjs show T0001 --json`.
- Help: `node ai_studio/taskboard/cli.mjs help`.
- Change: `node ai_studio/taskboard/cli.mjs new project --title "..." --kind game --target games/<id>`,
  `node ai_studio/taskboard/cli.mjs new epic --title "..." --project P001`,
  `node ai_studio/taskboard/cli.mjs new task --title "..." --project P001 --epic E001 --priority P1`,
  `node ai_studio/taskboard/cli.mjs set T0001 --status doing --log "..." --json`.
- Close with structured evidence:
  `node ai_studio/taskboard/cli.mjs set T0001 --status done --quality "QTECH_001=pass" --quality-evidence "tests passed" --json`.
- Validate store shape: `node ai_studio/taskboard/cli.mjs validate --json`.
- Profile routing reads: `node ai_studio/taskboard/cli.mjs profile --json`.
  The profiler-owned adapter measures summary/context/explicit-show reads for
  every registered Taskboard-enabled public/private mount. Its serialized
  records contain store metadata, operation, path/query, UTF-8 bytes, median
  duration, truncation, and result count, never task titles or bodies.

Taskboard is store-qualified:

- The default store is public Studio state under `ai_studio/taskboard/items/`.
- Game stores live under `games/<id>/.ai_studio/taskboard/items/` and are
  discovered through `ai_studio/workspace/games.mjs`.
- Private game stores are excluded unless a command/API request names the store
  (`--store game:<id>`, `--game <id>`, or HTTP `x-ai-studio-store: game:<id>`)
  or explicitly asks for aggregate private visibility (`--include-private` or
  `?includePrivate=1`). `?store=game:<id>` and `?game=<id>` remain manual
  fallback API inputs, not the browser UI contract.
- Rows include `storeId`, `visibility`, and `qualifiedId`. Bare IDs remain valid
  inside a selected store; aggregate reads reject ambiguous bare IDs.

The browser board opens as a local aggregate view and shows a Store filter in
the toolbar. It does not require a path or query parameter to see mounted private
game stores. Creates go to the one selected store, or to Studio when the view is
still aggregate; updates route by the edited row's store identity.

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
- Use `ai_studio/taskboard/stores.mjs` when a caller needs workspace-mounted
  Studio/game store selection or aggregate store-qualified payloads.
- Do not import the HTTP adapter for payload construction.
- Do not treat private store details as public API.

Internal module ownership is intentionally small:

- `store.mjs`: markdown store, frontmatter parse/serialize, path layout,
  constants, templates, create/update/list/find, archive movement, stable JSON
  payloads, and validation.
- `stores.mjs`: workspace store resolver for Studio/game Taskboard stores,
  explicit private inclusion, qualified IDs, and aggregate payloads.
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

For substantial work: `node ai_studio/taskboard/cli.mjs summary --json` -> an
explicitly scoped `list` query or `show <id>` -> needed evidence files ->
`games/<game-id>/` only for game-specific work -> one matching skill. The
default summary/context payload is capped at five body-free task rows; only
`show` returns a task body.

Search current scope only. Avoid archives, P3 ideas, broad design, and build
artifacts unless linked.

## Done And Validation

A new task transition to `done` is guarded by the closure and quality-decision
contract in `task-store-reference.md`. Existing `done` history is grandfathered.
Use the guide for lifecycle, evidence, CLI options, and canonical log formats.

Validation by change type: `ai_studio/quality/README.md`.
Repeated quality failures should be visible in task logs and summarized with
`node ai_studio/quality/profile.mjs`.

Taskboard validation checks task-store structure only.

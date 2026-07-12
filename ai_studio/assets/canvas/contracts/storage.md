# Canvas storage contract

Projects are plain directories containing `project.json`, append-only
`journal.jsonl`, sidecar snapshots, immutable `files/`, and generated exports.
The store uses per-project locking and atomic metadata replacement. Do not hand
edit project files; use `ops.mjs` or the CLI.

`ai_studio/studio.config.json` contains portable defaults. Machine-specific
roots belong in ignored `ai_studio/studio.config.local.json`, whose fields
override the tracked file. `CANVAS_PROJECTS_ROOT` remains the explicit test and
one-off override. The default repo-local projects directory is ignored.

Shared Canvas storage remains external in normal workstation configuration.
Project ownership is metadata (`ownership.kind=game`, `gameId`), not a game-side
copy of project data.

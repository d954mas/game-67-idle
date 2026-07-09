# Games

Working games live here as `games/<game-id>/`.

There can be multiple games at the same time. Each game is an independent copy
created from a template or another explicit source folder, then customized in
place.

Register public/tracked active game asset roots in
`games/games.json`. This lets Asset Viewer and asset tools show public
game-local assets alongside template and library sources.
Use `node games/new_game.mjs --id <game-id>` when creating a new
game; it registers the game, creates/reuses a Taskboard project, lays down the
game-owned `design/` scaffold, and refreshes VS Code build/run entries.

Private commercial games can be mounted locally through the ignored registry
`ai_studio/workspace/games.local.json`; see `ai_studio/workspace/README.md`.
Do not put private game ids, remotes, task logs, canvas refs, or evidence paths
in `games/games.json` or other tracked Studio files.

Each game owns its private design knowledge base under
`games/<game-id>/design/knowledge/`. Keep accepted game-specific facts, reference
lessons, playtest findings, and build observations there. Keep reusable
cross-game rules in the shared game-design knowledge base.

`games/games.json` is not a record of which template a game came from. Template
choice is only used at creation time because the template is copied.

Reusable feature packs can be copied from `features/`, but after copying the
game owns and may customize its local feature code, assets, and state.

`.vscode/tasks.json` and `.vscode/launch.json` are generated from public
`games/games.json` and `templates/templates.json`. Private games need explicit
workspace activation before any private-aware generator may include them.

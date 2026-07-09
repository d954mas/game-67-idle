# Games

Working games live here as `games/<game-id>/`.

There can be multiple games at the same time. Each game is an independent copy
created from a template or another explicit source folder, then customized in
place.

Register public/tracked active game asset roots in
`games/games.json`. This lets Asset Viewer and asset tools show public
game-local assets alongside template and library sources.
Use an explicit visibility choice when creating a new game:

```powershell
node games/new_game.mjs --id <game-id> --visibility public
```

The public/tracked flow registers the game in `games/games.json`,
creates/reuses a parent Taskboard project, lays down the game-owned `design/`
and `.ai_studio/` scaffolds, and refreshes VS Code build/run entries.

Private commercial games must be explicit:

```powershell
node games/new_game.mjs --id <private-game-id> --visibility private
```

The private flow creates `games/<private-game-id>/`, initializes or verifies its
nested Git repository, creates the game-owned `.ai_studio/` scaffold, updates
only local ignored registry/exclude files, and skips public `games/games.json`,
parent Taskboard, parent Canvas, and generated `.vscode` entries. Private games
can then be mounted locally through the ignored registry
`ai_studio/workspace/games.local.json`; see `ai_studio/workspace/README.md`.
Do not put private game ids, remotes, task logs, canvas refs, or evidence paths
in `games/games.json` or other tracked Studio files.

For backward compatibility, omitting `--visibility` still creates a public
tracked game. Human-facing, agent-facing, or Studio browser flows should add
`--require-visibility` so a missing public/private choice fails before any game
files are copied. `--private` remains a compatibility alias for
`--visibility private`.

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

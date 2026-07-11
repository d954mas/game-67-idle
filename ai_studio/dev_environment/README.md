# Dev Environment

Local developer-environment helpers for this AI Studio workspace.

This module owns generated local IDE wiring, not game/template creation and not
runtime automation. Its current public command regenerates VS Code tasks and
launch configs from explicit public template and game registries:

```powershell
node ai_studio/dev_environment/vscode_projects.mjs
```

Generated outputs:

- `.vscode/tasks.json`
- `.vscode/launch.json`

These files are tracked parent-Studio outputs. They are generated from
the tracked `ai_studio/workspace/catalog.json` only, and must not read
`ai_studio/workspace/catalog.local.json` or include private mount ids, paths, or
remotes. Private game IDE entries belong inside the private game repository or
in ignored local workspace files after `node ai_studio/workspace/games.mjs
preflight --json` passes.

To generate VS Code wiring for one mounted private game, write inside that game
repo:

```powershell
node ai_studio/dev_environment/vscode_projects.mjs --game <game-id>
```

This resolves `game:<game-id>` through `ai_studio/workspace/catalog.local.json`,
runs the workspace preflight, and writes `games/<game-id>/.vscode/tasks.json`
plus `games/<game-id>/.vscode/launch.json`. It does not touch parent
`.vscode/`.

Games and templates call this generator after creating a project, but the
project creation commands live with their workspace roots:

- `games/new_game.mjs`
- `templates/new_template.mjs`

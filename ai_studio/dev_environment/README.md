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
`templates/templates.json` and `games/games.json` only, and must not read
`ai_studio/workspace/games.local.json` or include private mount ids, paths, or
remotes. Private game IDE entries belong inside the private game repository or
in ignored local workspace files after `node ai_studio/workspace/games.mjs
preflight --json` passes.

Games and templates call this generator after creating a project, but the
project creation commands live with their workspace roots:

- `games/new_game.mjs`
- `templates/new_template.mjs`

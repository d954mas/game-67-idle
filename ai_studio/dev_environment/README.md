# Dev Environment

Local developer-environment helpers for this AI Studio workspace.

This module owns generated local IDE wiring, not game/template creation and not
runtime automation. Its current public command regenerates VS Code tasks and
launch configs from explicit template and game registries:

```powershell
node ai_studio/dev_environment/vscode_projects.mjs
```

Generated outputs:

- `.vscode/tasks.json`
- `.vscode/launch.json`

Games and templates call this generator after creating a project, but the
project creation commands live with their workspace roots:

- `games/new_game.mjs`
- `templates/new_template.mjs`

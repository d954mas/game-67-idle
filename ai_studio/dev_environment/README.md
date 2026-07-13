# Dev Environment

Local developer-environment helpers for this AI Studio workspace.

## Studio Python

Every ordinary Studio Python command resolves the root `.venv` named by
`studio.config.json.pythonPath` through `studioPythonPath()`. The resolver picks
`Scripts/python.exe` on Windows and `bin/python` on Linux/macOS. There is no
PATH, `py`, system-Python, or specialist environment fallback.

```powershell
node ai_studio/dev_environment/python_check.mjs
node ai_studio/dev_environment/python_run.mjs -m unittest discover -s ai_studio/runtime_automation -p "*_test.py"
```

Top-level ordinary Studio requirements use exact pins in
`ai_studio/python/requirements.direct.txt`; `python_check.mjs` verifies those
direct distributions. pip resolves transitive dependencies, so this is not a
reproducible full dependency lock. To create or repair the environment, first
install/repair Python 3.12 for the current Windows user, then pass that known
executable once:

```powershell
winget install --id Python.Python.3.12 --scope user
# Disable stale Microsoft Store python aliases in Windows "App execution aliases" if needed.
node ai_studio/dev_environment/python_setup.mjs --base-python "C:\Path\To\Python312\python.exe"
```

Normal commands use only `.venv` after bootstrap. ComfyUI embedded Python,
CorridorKey, VitMatte, MatAnyone, Blender, and SDK-owned interpreters are
explicit specialist environments, never fallback candidates.

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

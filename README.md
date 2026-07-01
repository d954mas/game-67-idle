# AI game-dev pipeline

The repo root is the **shared pipeline**, not a game: the engine (submodule),
build/asset/validation tools, AI skills, docs, the taskboard, and reusable design
knowledge. Templates live under `templates/`; reusable feature packs live under
`features/`; each **game is its own folder** under `games/`, copied from a template.

- Reusable AI workflow: `ai_studio/README.md`, `AGENTS.md`, `ai_studio/taskboard/`, `.codex/skills/`.
- Shared reusable gamedev knowledge: `ai_studio/game_design/knowledge_base/knowledge/`; sources `ai_studio/game_design/knowledge_base/sources/`.
- Engine: git submodule at `external/neotolis-engine` (public APIs only).
- Shared asset library lives OUTSIDE the repo (private); games pull project-local
  copies. Paid/licensed binaries never enter git - see the restricted-asset rule
  in `AGENTS.md` + `ai_studio/assets/storage/license/`.
- Reusable feature packs: `features/` (copy into a template or game, then customize).
- Game-specific docs, GDDs, private knowledge, assets, runtime code, and state
  live under `games/<game-id>/`.
- Closed prototypes are preserved as git tags.

## Start a new game

```powershell
git submodule update --init --recursive
node games/new_game.mjs --id <game-id>   # copies templates/template/ -> games/<game-id>/
```

Then customise the copy and pull assets/features. The new game starts with
`design/concept.md`, `design/gdd.md`, `design/knowledge/`, and starter structured
design data, plus the runnable template shell. The `templates/template/` folder
itself is the runnable reference (settings UI on nt_ui widgets, coloured +
textured mesh paths, a movement system, screenshot capture).

## Build + run a game (template shown)

VS Code users can use Run and Debug entries such as
`Debug Template: template (native debug)`. Build, pack, and run tasks are
generated from `templates/templates.json` and `games/games.json` by
`node ai_studio/dev_environment/vscode_projects.mjs`; `games/new_game.mjs` and
`templates/new_template.mjs` refresh them after creating a project.

```powershell
cmake -S templates/template -B templates/template/build/native-debug -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug
cmake --build templates/template/build/native-debug --target game
templates/template/build/native-debug/bin/game.exe                                # window: cubes + text + Settings
templates/template/build/native-debug/bin/game.exe --settings --capture tmp/x.ppm # headless screenshot
```

Layout a game folder teaches by example: `src/main.c` is the conductor; systems in
`src/systems/`, render in `src/render/`, UI in `src/ui/` (styles in their own
file), world state in `src/world/`. See `templates/template/CONVENTIONS.md` and
`templates/TEMPLATE.md`.

## Validate the pipeline

```powershell
node ai_studio/taskboard/cli.mjs validate     # docs/tasks
node ai_studio/core_harness/validation/doc_reference_check.mjs  # agent-facing docs/routes
```

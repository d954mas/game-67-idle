# AI game-dev pipeline

The repo root is the **shared pipeline**, not a game: the engine (submodule),
build/asset/validation tools, AI skills, docs, the taskboard, and reusable design
knowledge. Each **game is its own folder**, copied from `template/`.

- Reusable AI workflow: `ai_studio/README.md`, `AGENTS.md`, `tasks/`, `.codex/skills/`.
- Reusable design knowledge: `gamedesign/knowledge/`; sources `gamedesign/sources/`.
- Engine: git submodule at `external/neotolis-engine` (public APIs only).
- Shared asset library lives OUTSIDE the repo (private); games pull project-local
  copies. Paid/licensed binaries never enter git — see the restricted-asset rule
  in `AGENTS.md` + `ai_studio/assets/storage/license/`.
- Closed prototypes are git tags (e.g. `blockside-heat-snapshot-2026-06-24`).

## Start a new game

```powershell
git submodule update --init --recursive
node tools/bootstrap/new_game.mjs --id <game-id>   # copies template/ -> <game-id>/
```

Then customise the copy and pull assets/systems. The `template/` folder itself is
the runnable reference (settings UI on nt_ui widgets, coloured + textured mesh
paths, a movement system, screenshot capture).

## Build + run a game (template shown)

```powershell
cmake -S template -B template/build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug
cmake --build template/build
template/build/bin/game.exe                                   # window: cubes + text + Settings
template/build/bin/game.exe --settings --capture tmp/x.ppm    # headless screenshot
```

Layout a game folder teaches by example: `src/main.c` is the conductor; systems in
`src/systems/`, render in `src/render/`, UI in `src/ui/` (styles in their own
file), world state in `src/world/`. See `template/CONVENTIONS.md` and
`tools/bootstrap/TEMPLATE.md`.

## Validate the pipeline

```powershell
node ai_studio/taskboard/cli.mjs validate     # docs/tasks
node ai_studio/core_harness/validation/doc_reference_check.mjs  # agent-facing docs/routes
```

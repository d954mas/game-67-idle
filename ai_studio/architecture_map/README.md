# Architecture Map

This module owns the visual AI Studio architecture map.

The map is data-driven:

- `../tree.json` is the architecture source of truth. It is a small **index**:
  it keeps the root-node metadata plus a `root.parts` list of per-child files
  under `tree/`. Each top-level workspace child (one module or workspace group)
  is one JSON file in `tree/`, so edits localize to a single part.
- `tree/` holds the split parts. Edit the matching part file, not one giant
  tree; add or reorder a top-level child by editing the `parts` list in
  `../tree.json`.
- `tree_loader.mjs` merges the index and its parts back into one tree. A legacy
  single-file tree (`root.children` with no `root.parts`) is returned unchanged.
- `index.html` renders the map in the browser. It loads the merged tree from
  `GET /api/architecture-tree` (falling back to reading `../tree.json` and
  merging `tree/` parts client-side).
- `validate_map.mjs` merges the tree, derives coverage from `git ls-files`,
  scans AI Studio ownership locations and shallow workspace roots, and writes a local `validation-report.json`
  for offline inspection. That file is **git-ignored, not committed**. Local
  private game mounts from `ai_studio/workspace/catalog.local.json` are excluded
  from parent architecture scans so their ids and paths do not appear in the
  generated report.
- `api.mjs` is the Studio Shell adapter. `GET /api/architecture-validation`
  returns a freshly computed report so the page never depends on a committed
  file; `GET /api/architecture-tree` returns the merged tree.
- `../studio_shell/server.mjs` hosts the map surface and mounts `api.mjs` so
  browser `fetch()` can read the merged tree and the live report.

The page must not infer architecture from the repository. New tracked files are
not silently added to the map. They appear in validation until a human decides
whether to map, ignore, open a task, or delete them.

Untracked or generated local files are not architecture coverage truth. Pass
`--hygiene` to include them in a separate non-gating `hygiene.untrackedPaths`
report when local cleanup is relevant.

`ai_studio/game_design/knowledge_base/` is owned by the Game Design module and is
mapped there as one covered folder. `templates/`, `games/`, and `extensions/`
are shown as workspace containers; `features/` is shown as a feature group. They use
`coverage: "self"` so the container path is checked without automatically
covering every child folder. Validation scans tracked files directly under
those roots and their immediate child directories. Root-level commands and docs
such as `games/new_game.mjs` or `templates/new_template.mjs` therefore appear
in validation if they are not mapped, while new `templates/<id>`,
`features/<id>`, `games/<id>`, or top-level extension folders appear as unmapped
outside-AI-Studio paths until they are intentionally added to `tree.json`. Files
inside each game, template, feature, or mapped extension folder are not listed
individually in the architecture map.

Taskboard data is not architecture map module data. The markdown store lives in
`ai_studio/taskboard/items/{projects,epics,active,archive}/` and is owned by the
Taskboard module. The map covers that folder as state, not as individual source
nodes.

## Display Types

Nodes in `../tree.json` use `kind` for both color and display filtering. Tests
use `kind: "test"` and validators or validation docs use `kind: "validation"`.
The map page's `Types` menu hides `test` and `validation` by default, but they
remain architecture data: they still map files and count in validation.

Node card colors are assigned by `kind`, not by owner: module, group, doc,
config, tool, skill, validation, test, folder, contract, and backlog each have a
stable color in the page legend.

## Commands

```powershell
node ai_studio/architecture_map/validate_map.mjs
node ai_studio/architecture_map/validate_map.mjs --hygiene
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

Default URL:

```text
http://127.0.0.1:8765/architecture_map/
```

## Validation Categories

- `missingInRepo`: a path is listed in `tree.json`, but the file or directory
  is absent.
- `duplicateMappings`: a path is listed by more than one map node.
- `unmappedInAiStudio`: a file exists under `ai_studio/`, but is not listed or
  covered by a mapped directory in `tree.json`.
- `unmappedOutsideAiStudio`: a scanned path exists outside `ai_studio/` and is
  not explicitly mapped. For `templates/`, `features/`, `games/`, and `extensions/`, scanned
  paths are root-level tracked files plus immediate child directories, not files
  inside those child directories.
- `missingDescriptions`: a visible node lacks a useful description.
- `invalidDescriptions`: a description exceeds 240 characters or contains
  command syntax, routes, test-case detail, or UI micro-behavior instead of an
  architectural responsibility.

Scanning is validation only. It does not edit `tree.json`.

Before sharing architecture validation output or running a workflow that turns
local reports into tracked evidence, run:

```powershell
node ai_studio/workspace/games.mjs preflight --json
```

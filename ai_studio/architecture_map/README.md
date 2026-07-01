# Architecture Map

This module owns the visual AI Studio architecture map.

The map is data-driven:

- `../tree.json` is the architecture source of truth.
- `index.html` renders the map in the browser.
- `validate_map.mjs` scans AI Studio source locations, shallow workspace
  folder roots, and writes
  `validation-report.json`.
- `validation-report.json` is displayed by the page so unmapped, missing, or
  duplicated files are visible during refactoring.
- `../studio_shell/server.mjs` hosts the map surface so browser `fetch()` can
  read JSON files.

The page must not infer architecture from the repository. New files are not
silently added to the map. They appear in validation until a human decides
whether to map, ignore, assign to the review backlog, or delete them.

`ai_studio/game_design/knowledge_base/` is owned by the Game Design module and is
mapped there as one covered folder. `templates/` and `games/` are shown as
workspace containers; `features/` is shown as a feature group. They use
`coverage: "self"` so the container path is checked without automatically
covering every child folder. Validation scans tracked files directly under
those roots and their immediate child directories. Root-level commands and docs
such as `games/new_game.mjs` or `templates/new_template.mjs` therefore appear
in validation if they are not mapped, while new `templates/<id>`,
`features/<id>`, or `games/<id>` folders appear as unmapped outside-AI-Studio
paths until they are intentionally added to `tree.json`. Files inside each game,
template, or feature folder are not listed in the architecture map.

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
  not explicitly mapped. For `templates/`, `features/`, and `games/`, scanned
  paths are root-level tracked files plus immediate child directories, not files
  inside those child directories.
- `missingDescriptions`: a visible node lacks a useful description.

Scanning is validation only. It does not edit `tree.json`.

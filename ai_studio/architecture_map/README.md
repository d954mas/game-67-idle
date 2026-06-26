# Architecture Map

This module owns the visual AI Studio architecture map.

The map is data-driven:

- `../tree.json` is the architecture source of truth.
- `index.html` renders the map in the browser.
- `validate_map.mjs` scans known AI Studio and legacy pipeline locations and
  writes `validation-report.json`.
- `validation-report.json` is displayed by the page so unmapped, missing, or
  duplicated files are visible during refactoring.
- `../studio_shell/server.mjs` hosts the map surface so browser `fetch()` can
  read JSON files.

The page must not infer architecture from the repository. New files are not
silently added to the map. They appear in validation until a human decides
whether to map, ignore, move to `Not Refactored`, or delete them.

## Display Types

Nodes in `../tree.json` use `kind` for both color and display filtering. Tests
use `kind: "test"` and validators or validation docs use `kind: "validation"`.
The map page's `Types` menu hides `test` and `validation` by default, but they
remain architecture data: they still map files and count in validation.

Node card colors are assigned by `kind`, not by owner: module, group, doc, tool,
skill, validation, test, folder, contract, and backlog each have a stable color
in the page legend.

## Commands

```powershell
node ai_studio/architecture_map/validate_map.mjs
node ai_studio/studio_shell/start_site.mjs --open
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
- `unmappedLegacy`: a legacy pipeline/tool/doc file exists outside `ai_studio/`
  and is not explicitly mapped.
- `missingDescriptions`: a visible node lacks a useful description.

Scanning is validation only. It does not edit `tree.json`.

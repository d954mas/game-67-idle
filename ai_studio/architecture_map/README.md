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

## Display Flags

Nodes in `../tree.json` may use `hiddenByDefault: true` to keep noisy files
such as tests or root index files out of the default graph view. Hidden nodes
are still architecture data: they still map files, count in validation, and can
be shown with the map page's `Show hidden` toggle.

Node card colors are assigned by `kind`, not by owner: module, group, doc, tool,
skill, folder, contract, and backlog each have a stable color in the page legend.

## Commands

```powershell
node ai_studio/architecture_map/validate_map.mjs
node ai_studio/studio_shell/server.mjs
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

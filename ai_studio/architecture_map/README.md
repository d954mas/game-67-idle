# Architecture Map

This module owns the compact AI Studio ownership hierarchy. It answers three
questions: what the durable modules are, who owns them, and where their public
boundary starts. It does not inventory implementation files.

## Source model

`../tree.json` is the single architecture source. There are no fragments,
generated leaves, or parallel relationship data.

Every node has:

- `id`, `title`, and `kind` for identity and display;
- `owner` and `description` for responsibility;
- `entry` and `verifyDomain` for the shortest route into the module and its
  focused proof;
- optional `path`, `contract`, `store`, and `boundary` locators;
- optional `children` for real owner boundaries;
- optional `coverage`: `subtree` (default), `direct-files`, or `self`.

`subtree` assigns all descendants to the node. `direct-files` assigns only
files immediately inside the mapped directory, so a new child module remains
unmapped. `self` maps only the exact path.

Tests, fixtures, generated output, histories, migrations, research notes, and
implementation helpers belong under their module's coverage. They are not map
nodes. Settings and the resource panel remain visible product features owned by
the default template; they do not need separate architecture leaves.

## Loading and validation

`tree_loader.mjs` reads only the configured repository-relative JSON file. It
fails on missing or malformed data, absolute or escaping paths, and obsolete
`parts` or `ref` references.

`validate_map.mjs` compares the map with tracked Git paths. The Studio root and
the `templates`, `features`, `games`, and `extensions` containers use
`direct-files`, while their known child modules are mapped explicitly. A new
top-level Studio module, template, feature pack, game, or extension therefore
fails strict validation until ownership is decided.

Local private game mounts are excluded before paths enter the report. Untracked
files are not ownership truth; `--hygiene` reports them separately when local
cleanup matters.

```powershell
node ai_studio/architecture_map/validate_map.mjs --strict
node ai_studio/architecture_map/validate_map.mjs --hygiene
```

Strict validation fails for missing or duplicate mappings, missing authored
`entry`/`contract`/`store` locators, unmapped tracked paths, invalid node fields
or locators, unknown coverage or verification domains, and operational
descriptions that do not state architectural intent. Private-mount discovery
errors block validation rather than falling back to a report that could expose
private paths.
Validation never edits the map.

## Surface and API

`index.html` renders the same hierarchy with drill-down, breadcrumbs, owner
types, and local card positions. It first reads `GET /api/architecture-tree`
and falls back to the same `../tree.json` file. It has no fragment loader or
second normalized architecture model.

`api.mjs` provides:

- `GET /api/architecture-tree` — the single authored tree;
- `GET /api/architecture-validation` — a fresh validation report.

The generated `validation-report.json` is local and ignored by Git.

The canonical Windows launcher and default page are documented by
`../studio_shell/README.md`; the map route is
`http://127.0.0.1:8765/architecture_map/`.

# Items Workbench

Studio surface for a registered template or game Items catalog. The current
Workbench shows a compact master table plus bounded Snapshot
detail, level/cost/provenance rows, selected-series charts, release state,
diagnostics, dependencies, checked source locations, and built icon previews.
Semantic preview/apply is delegated to the same T0366 CLI operations used by
agents; the browser owns no Lua writer. The editor offers only fields whose
Snapshot provenance matches the selected literal, curve, or override operation.
Preview state is visibly ephemeral, Apply resubmits the reviewed patch with its
expected source hash, and session Undo replays only the inverse patch returned
by the CLI.

## Boundary

The Viewer does not parse authored catalog data. `ops.mjs` invokes the
single-source semantic CLI with an explicit game root:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> list
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> validate
```

`list` supplies compact master rows and `validate` supplies
Snapshot/requirements/release-receipt diagnostics. A selected item invokes one
bounded `detail` composition of inspect/schema/source/dependencies; one selected
chart field invokes `chart` lazily. The Workbench adapts those
results to HTTP without a second evaluator or browser-side Items model; it
never reads `items.json`, the old field schema, or the legacy op-layer, and it
does not invent a second schema for rendering.

Concrete runtime containers are E019 state, not catalog definitions, so this
surface does not display a catalog container table.

## Files

- `ops.mjs` resolves registered catalogs, invokes the semantic CLI, projects
  release status, and attaches built icon previews.
- `api.mjs` exposes the focused HTTP routes.
- `site/` renders the responsive master/detail view with bare ESM and no build
  step.
- `tests/` covers the Lua route, private mount visibility, receipt status,
  invalid-source degradation, icon pack parsing, and tool failures.

## HTTP

- `GET /api/items-viewer/catalogs` lists registered catalogs. Private games
  require `?include-private=true` and still pass workspace privacy checks.
- `GET /api/items-viewer/catalog?id=<kind>:<id>` returns one complete view.
- `GET /api/items-viewer/icon-page?catalog=<kind>:<id>` serves the bounded built
  atlas PNG referenced by catalog metadata; catalog JSON never embeds base64.
- `GET /api/items-viewer/item?catalog=<kind>:<id>&item=<def_id>` returns one
  bounded `detail` result from one evaluator process.
- `GET /api/items-viewer/chart?catalog=<kind>:<id>&item=<def_id>&field=<member>`
  returns only the selected generated numeric series.
- `POST /api/items-viewer/edit` accepts exactly a registered catalog, one
  `items.cli.patch.v1`, and `apply`. `apply: false` is an ephemeral what-if;
  `apply: true` uses the CLI's expected-hash/lock/validation/atomic-replace
  path. Returned inverse patches are replayed through the same endpoint.

A folder with no `items.lua.json` is a valid empty state. Invalid Lua or a
Snapshot failure returns a top-level `content_error` with no JSON fallback.
Validation incompatibility remains visible through `validate.ok: false`; a
tool/process failure is the only server error.

## Release status

`items.lock.json` is a separate release receipt, not catalog authoring data.
The Viewer reads it only to label current IDs as:

- `shipped` when present in `def_ids`;
- `removed` when present in `removed` but not `def_ids`;
- `draft` otherwise.

Removed receipt entries with no current item appear in the Removed section.
Semantic CLI validation remains authoritative for whether the receipt is
compatible; the Viewer does not duplicate those rules.

## Icon preview

`icon_preview.mjs` asynchronously reads capped built `game.ntpack` and generated
header inputs, then reads only the PNG header for catalog metadata. It caps file,
asset, atlas, page, region, vertex, image-dimension, and pixel counts before
using offsets. The focused icon-page route separately serves the capped PNG.
The parser preserves 64-bit hashes as `BigInt` and degrades to a reason when
build artifacts are missing. The browser decodes the shared atlas image once
and crops every item icon from it.

The preferred long-term replacement is a native Studio adapter over the
engine's public atlas reader; the current JS binary reader stays isolated in
this module.

## Verify

```powershell
node --test ai_studio/assets/items_viewer/tests/api.test.mjs ai_studio/assets/items_viewer/tests/icon_preview.test.mjs ai_studio/assets/items_viewer/tests/ops.test.mjs ai_studio/assets/items_viewer/tests/site_model.test.mjs
node ai_studio/studio.mjs verify --changed
```

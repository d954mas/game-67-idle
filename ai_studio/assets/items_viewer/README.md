# Items Viewer

Read-only Studio surface for a registered template or game Items catalog. It
shows bounded Snapshot summaries, release-receipt status, validation issues,
and built icon previews. Definition editing belongs to T0316.

## Boundary

The Viewer does not parse authored catalog data. `ops.mjs` invokes the
single-source semantic CLI with an explicit game root:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> list
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> validate
```

`list` supplies explicit card metadata without level tables or acquire
transitions. `validate` supplies Snapshot/requirements/release-receipt
diagnostics. The Viewer adapts those bounded results to its HTTP shape; it
never reads `items.json`, the old field schema, or the legacy op-layer, and it
does not invent a second schema for rendering.

Concrete runtime containers are E019 state, not catalog definitions, so this
surface does not display a catalog container table.

## Files

- `ops.mjs` resolves registered catalogs, invokes the semantic CLI, projects
  release status, and attaches built icon previews.
- `api.mjs` exposes the read-only HTTP routes.
- `site/` renders the view with bare ESM and no build step.
- `tests/` covers the Lua route, private mount visibility, receipt status,
  invalid-source degradation, icon pack parsing, and tool failures.

## HTTP

- `GET /api/items-viewer/catalogs` lists registered catalogs. Private games
  require `?include-private=true` and still pass workspace privacy checks.
- `GET /api/items-viewer/catalog?id=<kind>:<id>` returns one complete view.

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

`icon_preview.mjs` reads a built `game.ntpack`, the generated asset header, and
the icons atlas debug PNG. It version-checks the pack/atlas formats, preserves
64-bit hashes as `BigInt`, and degrades to a reason when build artifacts are
missing. The page decodes the atlas image once and crops every item icon from
that shared image.

The preferred long-term replacement is a native Studio adapter over the
engine's public atlas reader; the current JS binary reader stays isolated in
this module.

## Verify

```powershell
node --test ai_studio/assets/items_viewer/tests/
node ai_studio/studio.mjs verify --changed
```

# Asset Viewer (Gallery)

Browser-facing asset browsing and review module.

## Role

The gallery creates inspectable galleries for the shared library or a game's
assets. It is a user-facing surface plus supporting commands; it does not own
the whole asset pipeline. It is the source-first browsing keystone: search the
shared library here before generating anything new.

Studio Shell hosts the launcher at:

```text
http://127.0.0.1:8765/asset_viewer/
```

`/viewer/` is also served as a short alias, but `/asset_viewer/` remains the
stable public route.

The gallery header lets the user choose which source to display:

- All Assets: the shared manifest-backed asset library from
  `ai_studio/assets/sources/libraries.json`.
- Templates: registered template asset roots from
  the workspace catalog and template identity manifests.
- Registered Games: public/tracked game-local asset folders from
  the workspace catalog and game identity manifests.

The generated static-gallery command remains for review exports and standalone
sharing, but it is not the normal browser entry.

Public games created through
`games/new_game.mjs --id <game-id> --visibility public` are registered
automatically as `<game-id>/assets`. Private games stay out of the tracked workspace catalog
unless mounted through a private-aware workspace flow. The gallery does not scan
root folders to guess games.

Private game sources are hidden from the source list by default. Explicit
private browsing uses the workspace game resolver (`game:<id>` /
`include-private`) and runs the private game preflight before exposing the
source. Do not open private game assets in the gallery by passing raw
`games/<id>/assets` paths; select the mounted game id instead. Generated gallery
metadata, indexes, and previews stay under ignored `tmp/` paths.

The gallery does not run filesystem watch mode. When assets are added or
changed locally, use the `Refresh` button. It checks the selected source
signature and only rebuilds the generated SQLite index when manifest metadata
or source files changed. This keeps the viewer predictable and avoids background
watcher state.

The gallery does not own Blender preview rendering. Prepared preview images
come from `../previews/`. Use `Refresh previews` after
adding or changing assets; it creates or replaces only missing/stale preview
cache files for the selected source, renders model thumbnails when Blender can
process the model, and then refreshes that source index. Generated previews and
their `preview.json` sidecars live under `tmp/` and are not committed.

Large library views use a scroll feed. The API still returns bounded batches,
but the user does not manage pages or press a manual "show more" button. When
the feed approaches the end of the loaded items, the gallery asks the local API
for the next slice. Those reads are served from `../catalog/`,
not by rescanning the asset source for each request.

Template and game-local sources use the same index API. They have their own
generated SQLite database and are queried through the same `/api/asset-viewer/assets`
endpoint as the shared library.

## Commands

```powershell
node ai_studio/assets/gallery/build_review.mjs --mode library
node ai_studio/assets/gallery/build_review.mjs --mode review --game <id> --base <git-ref>
node ai_studio/assets/gallery/build_review.mjs --mode scan --path <asset-dir>
node ai_studio/assets/gallery/serve_gallery.mjs --gallery tmp/lib-gallery --lib <library-root> --port 8910
node ai_studio/assets/previews/render_library_previews.mjs --pack <pack>
```

Promotion and reuse:

```powershell
node ai_studio/assets/gallery/promote.mjs --manifest <review-manifest.json> --ids <ids> --source <source> --license <license>
node ai_studio/assets/gallery/pull.mjs --ids <asset-ids> --to <game>/assets
```

## Ownership

- `build_review.mjs` generates the static gallery output and review manifest.
- `index.html` is the hosted gallery entry.
- `api.mjs` lists available sources, opens the selected source in the gallery,
  rebuilds/queries `../catalog/`, and maps gallery media back
  into the Studio Shell server.
- The gallery consumes sources through `../sources/ops.mjs` and workspace
  helpers. Global library data lives in `../sources/libraries.json`; template
  and game roots are discovered from identity manifests.
- `../sources/ops.mjs` and `../../workspace/games.mjs` expose those sources to
  the gallery and CLI commands.
- `viewer.js` and `viewer.css` are surface implementation details.
- `serve_gallery.mjs` is kept because library mode can reference a large
  external asset library through `/lib/` instead of copying every model.
- `promote.mjs` moves selected project assets into Pack Manifest storage.
- `pull.mjs` copies selected shared-library assets into a game-local asset tree.
- `record_gallery.mjs` captures a browser session of the gallery.

The broader intake, conversion, and validators are separate asset modules. The
raster image tools (region detection, slicing, alpha) live in
`../tools/image/` and the multi-image canvas editor lives in `../canvas/`.

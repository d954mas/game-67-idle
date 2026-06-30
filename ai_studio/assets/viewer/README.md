# Asset Viewer

Browser-facing asset review module.

## Role

Asset Viewer creates inspectable galleries for the shared library or a game's
assets. It is a user-facing surface plus supporting commands; it does not own
the whole asset pipeline.

Studio Shell hosts the launcher at:

```text
http://127.0.0.1:8765/asset_viewer/
```

`/viewer/` is also served as a short alias, but `/asset_viewer/` remains the
stable public route.

The gallery header lets the user choose which source to display:

- All Assets: the shared manifest-backed asset library from
  `ai_studio/assets/storage/sources/libraries.json`.
- Templates: registered template asset roots from
  `ai_studio/assets/storage/sources/templates.json`.
- Current Game: the active game assets folder from `GAME_PROJECT.md`, when one
  is set.
- Registered Games: game-local asset folders from
  `ai_studio/assets/storage/sources/games.json`.

The generated static-gallery command remains for review exports and standalone
sharing, but it is not the normal browser entry.

New games created through `ai_studio/bootstrap/new_game.mjs --id <game-id>` are
registered automatically as `<game-id>/assets`. Asset Viewer does not scan root
folders to guess games.

Asset Viewer does not run filesystem watch mode. When assets are added or
changed locally, use the `Refresh` button. It checks the selected source
signature and only rebuilds the generated SQLite index when manifest metadata
or source files changed. This keeps the viewer predictable and avoids background watcher
state.

Asset Viewer does not own Blender preview rendering. Prepared preview images
come from `../storage/previews/`. Use `Refresh previews` after
adding or changing assets; it creates or replaces only missing/stale preview
cache files for the selected source, renders model thumbnails when Blender can
process the model, and then refreshes that source index. Generated previews and
their `preview.json` sidecars live under `tmp/` and are not committed.

Large library views use a scroll feed. The API still returns bounded batches,
but the user does not manage pages or press a manual "show more" button. When
the feed approaches the end of the loaded items, Asset Viewer asks the local API
for the next slice. Those reads are served from `../storage/index/`,
not by rescanning the asset source for each request.

Template and game-local sources use the same index API. They have their own
generated SQLite database and are queried through the same `/api/asset-viewer/assets`
endpoint as the shared library.

## Commands

```powershell
node ai_studio/assets/viewer/build_review.mjs --mode library
node ai_studio/assets/viewer/build_review.mjs --mode review --game <id> --base <git-ref>
node ai_studio/assets/viewer/build_review.mjs --mode scan --path <asset-dir>
node ai_studio/assets/viewer/serve_gallery.mjs --gallery tmp/lib-gallery --lib <library-root> --port 8910
node ai_studio/assets/storage/previews/render_library_previews.mjs --pack <pack>
```

Promotion and reuse:

```powershell
node ai_studio/assets/viewer/promote.mjs --manifest <review-manifest.json> --ids <ids> --source <source> --license <license>
node ai_studio/assets/viewer/pull.mjs --ids <asset-ids> --to <game>/assets
```

## Ownership

- `build_review.mjs` generates the static gallery output and review manifest.
- `index.html` is the hosted gallery entry.
- `api.mjs` lists available sources, opens the selected source in the gallery,
  rebuilds/queries `../storage/index/`, and maps gallery media back
  into the Studio Shell server.
- `../storage/sources/libraries.json` is the explicit list of
  registered global library roots.
- `../storage/sources/templates.json` is the explicit list of
  registered template asset roots.
- `../storage/sources/games.json` is the explicit list of
  registered game asset roots.
- `../storage/sources/*_registry.mjs` reads and updates those
  registries.
- `viewer.js` and `viewer.css` are surface implementation details.
- `serve_gallery.mjs` is kept because library mode can reference a large
  external asset library through `/lib/` instead of copying every model.
- `promote.mjs` moves selected project assets into Pack Manifest storage.
- `pull.mjs` copies selected shared-library assets into a game-local asset tree.
- `record_gallery.mjs` captures a browser session of the gallery.

The broader intake, conversion, and validators are separate asset modules.

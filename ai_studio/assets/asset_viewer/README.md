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

The gallery header lets the user choose which source to display:

- All Assets: the shared catalog-backed asset library.
- Templates: registered template asset roots from
  `ai_studio/assets/assets_storage/source_registry/templates.json`.
- Current Game: the active game assets folder from `GAME_PROJECT.md`, when one
  is set.
- Registered Games: game-local asset folders from
  `ai_studio/assets/assets_storage/source_registry/games.json`.

The generated static-gallery command remains for review exports and standalone
sharing, but it is not the normal browser entry.

New games created through `tools/bootstrap/new_game.mjs --id <game-id>` are
registered automatically as `<game-id>/assets`. Asset Viewer does not scan root
folders to guess games.

Asset Viewer does not run filesystem watch mode. When assets are added or
changed locally, use the `Refresh` button. For the shared library it rebuilds
the asset index; for template/game folders it clears the source cache and
rescans the folder. This keeps the viewer predictable and avoids background
watcher state.

Asset Viewer does not own Blender preview rendering. Prepared preview images
come from `../assets_storage/preview_pipeline/`. Use `Refresh previews` after
adding or changing assets; it creates missing preview cache files for the
selected source, renders model thumbnails when Blender can process the model,
and then rebuilds that source index. Generated previews live under `tmp/` and
are not committed.

Large library views are paged by the server. `All Assets` can contain thousands
or tens of thousands of items, so the browser asks the local API for the current
slice, search result, filter result, or next page instead of loading the whole
asset list into the page. Those reads are served from
`../assets_storage/asset_index/`, not by rescanning the OKF catalog for each
request.

Template and game-local sources use the same index API. They have their own
generated SQLite database and are queried through the same `/api/asset-viewer/assets`
endpoint as the shared library.

## Commands

```powershell
node ai_studio/assets/asset_viewer/build_review.mjs --mode library
node ai_studio/assets/asset_viewer/build_review.mjs --mode review --game <id> --base <git-ref>
node ai_studio/assets/asset_viewer/build_review.mjs --mode scan --path <asset-dir>
node ai_studio/assets/asset_viewer/serve_gallery.mjs --gallery tmp/lib-gallery --lib <library-root> --port 8910
node ai_studio/assets/assets_storage/preview_pipeline/render_library_previews.mjs --pack <pack>
```

Promotion and reuse:

```powershell
node ai_studio/assets/asset_viewer/promote.mjs --manifest <review-manifest.json> --ids <ids> --source <source> --license <license>
node ai_studio/assets/asset_viewer/pull.mjs --ids <asset-ids> --to <game>/assets
```

## Ownership

- `build_review.mjs` generates the static gallery output and review manifest.
- `index.html` is the hosted gallery entry.
- `api.mjs` lists available sources, opens the selected source in the gallery,
  rebuilds/queries `../assets_storage/asset_index/`, and maps gallery media back
  into the Studio Shell server.
- `../assets_storage/source_registry/templates.json` is the explicit list of
  registered template asset roots.
- `../assets_storage/source_registry/games.json` is the explicit list of
  registered game asset roots.
- `../assets_storage/source_registry/*_registry.mjs` reads and updates those
  registries.
- `viewer.js` and `viewer.css` are surface implementation details.
- `serve_gallery.mjs` is kept because library mode can reference a large
  external asset library through `/lib/` instead of copying every model.
- `promote.mjs` moves selected project assets into the shared library.
- `pull.mjs` copies selected shared-library assets into a game-local asset tree.
- `record_gallery.mjs` captures a browser session of the gallery.

The broader intake, conversion, and validators are still owned by the legacy
`tools/assets/` area until reviewed separately.

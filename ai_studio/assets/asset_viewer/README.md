# Asset Viewer

Browser-facing asset review module.

## Role

Asset Viewer creates inspectable galleries for the shared library or a game's
assets. It is a user-facing surface plus supporting commands; it does not own
the whole asset pipeline.

## Commands

```powershell
node ai_studio/assets/asset_viewer/build_review.mjs --mode library
node ai_studio/assets/asset_viewer/build_review.mjs --mode review --game <id> --base <git-ref>
node ai_studio/assets/asset_viewer/build_review.mjs --mode scan --path <asset-dir>
node ai_studio/assets/asset_viewer/serve_gallery.mjs --gallery tmp/lib-gallery --lib <library-root> --port 8910
```

Promotion and reuse:

```powershell
node ai_studio/assets/asset_viewer/promote.mjs --manifest <review-manifest.json> --ids <ids> --source <source> --license <license>
node ai_studio/assets/asset_viewer/pull.mjs --ids <asset-ids> --to <game>/assets
```

## Ownership

- `build_review.mjs` generates the static gallery output and review manifest.
- `viewer.js` and `viewer.css` are surface implementation details.
- `serve_gallery.mjs` is kept because library mode can reference a large
  external asset library through `/lib/` instead of copying every model.
- `promote.mjs` moves selected project assets into the shared library.
- `pull.mjs` copies selected shared-library assets into a game-local asset tree.
- `record_gallery.mjs` captures a browser session of the gallery.

The shared asset catalog, source search, intake, conversion, and validators are
still owned by the legacy `tools/assets/` area until reviewed separately.


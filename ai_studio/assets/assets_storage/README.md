# Assets Storage

Reviewed storage layer for AI Studio assets.

## Role

Assets Storage owns asset source locations, catalog records, derived preview
artifacts, the local query index, and storage-facing preparation tools. It does
not own the browser gallery UI.

## Parts

- `source_registry/`: explicit local source registry. It records game asset
  folders that Asset Viewer can display.
- `okf_catalog/`: Markdown/OKF-style shared library reader and source-first
  search entrypoint.
- `asset_index/`: local SQLite indexes, one per source, built from OKF records
  or folder scans for fast page, search, filter, pack, and model lookups.
- `preview_pipeline/`: local derived-preview preparation. It copies image
  previews and renders model thumbnails into `tmp/ai_studio/assets/previews/`,
  plus owns the shared studio HDR.

## Boundary

- `../asset_viewer/` displays storage data through the index and asks for
  explicit refresh or regenerate actions.
- Storage tools may read or write the shared local asset library.
- No filesystem watch mode belongs here. Use page reload, manual refresh/reindex,
  or targeted preview refresh after local asset changes.
- Generated SQLite indexes and preview cache files live under `tmp/` and do not
  belong in git.
- Source intake, conversion, pack building, and broader asset validation still
  live in legacy `tools/assets/` until reviewed and moved into storage modules.

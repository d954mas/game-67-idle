# Asset Index

SQLite-backed local index for AI Studio asset browsing.

## Role

Asset Index turns each asset source into queryable local data for browser
surfaces and tools. It is the boundary between slow filesystem/catalog scans and
fast UI/API queries.

```text
asset roots
  global library
  template/assets
  current game/assets
  registered game assets
        |
asset index
  one SQLite database per source
  explicit rebuild / refresh
  pack summaries
  asset query
  facets
  model path lookup
        |
asset_viewer/
  UI only
```

## Storage

The first implementation uses the built-in `node:sqlite` runtime and writes
one generated database per source under:

```text
tmp/ai_studio/assets/asset_index/
```

Examples:

```text
tmp/ai_studio/assets/asset_index/global-library.sqlite
tmp/ai_studio/assets/asset_index/template.sqlite
tmp/ai_studio/assets/asset_index/game_<game-id>.sqlite
```

The database is local cache, not source of truth. The source of truth remains
the asset storage root: OKF Markdown catalog records for the shared library, or
the template/game asset folder for folder-backed sources.

## Public API

- `ensureAssetIndex(root, source)`: create or rebuild a missing index.
- `rebuildAssetIndex(root, source)`: full source rebuild.
- `queryIndexedAssets(root, source, query)`: paged assets, facets, totals.
- `listIndexedPacks(root, source)`: pack-first summaries for the viewer.
- `resolveIndexedModel(root, source, assetId)`: model path lookup.

Asset Viewer exposes the full rebuild through its `Refresh` action for every
source. Normal browsing should use `queryIndexedAssets` and `listIndexedPacks`.

## Refresh Policy

No filesystem watch mode. Updates are explicit:

- manual refresh/rebuild;
- page reload cheap freshness check in a later slice;
- targeted preview regenerate jobs in a later slice.

Future incremental refresh should compare candidate file `mtime` and size
against the index, parse only changed Markdown records, and update preview state
without blocking the browser page.

# Asset Index

SQLite-backed local index for AI Studio asset browsing.

## Role

Asset Index turns each asset source into queryable local data for browser
surfaces and tools. It is the boundary between slow filesystem/manifest scans
and fast UI/API queries.

```text
asset roots
  global library
  templates/template/assets
  current game/assets
  registered game assets
        |
source readers
  Pack Manifest reader
  raw folder scanner
        |
asset index
  one SQLite database per source
  explicit rebuild / refresh
  pack summaries
  asset query
  facets
  model path lookup
        |
viewer/
  UI only
```

## Storage

The first implementation uses the built-in `node:sqlite` runtime and writes
one generated database per source under:

```text
tmp/ai_studio/assets/index/
```

Examples:

```text
tmp/ai_studio/assets/index/global-library.sqlite
tmp/ai_studio/assets/index/template.sqlite
tmp/ai_studio/assets/index/game_<game-id>.sqlite
```

The database is local cache, not source of truth. The source of truth remains
the asset storage root: Pack Manifest metadata for reviewed sources, or the
template/game asset folder for raw folder-backed sources.

## Public API

- `ensureAssetIndex(root, source)`: create or rebuild a missing index.
- `refreshAssetIndex(root, source)`: explicit refresh that skips the full
  rebuild when the source snapshot signature is unchanged.
- `rebuildAssetIndex(root, source)`: full source rebuild.
- `queryIndexedAssets(root, source, query)`: paged assets, facets, totals.
- `listIndexedPacks(root, source)`: pack-first summaries for the viewer.
- `resolveIndexedModel(root, source, assetId)`: model path lookup.

Asset Viewer exposes `refreshAssetIndex` through its `Refresh` action for every
source. Normal browsing should use `queryIndexedAssets` and `listIndexedPacks`.

Unfiltered global facets are stored as generated counts during rebuild so the
first All Assets page does not need broad `GROUP BY` queries. Filtered,
searched, and pack-scoped facets are still computed from indexed rows at query
time.

## Refresh Policy

No filesystem watch mode. Updates are explicit:

- manual refresh checks `../snapshots/` manifest and asset-file
  `mtime`/size signatures, then
  rebuilds only when metadata or asset files changed;
- forced rebuild remains available through code when the source signature is not
  enough;
- targeted preview refresh lives in `../previews/` and owns preview
  changes.

The snapshot signature is a fast guard, not the source of truth. When it
changes, the index is rebuilt from Pack Manifest metadata or the folder scan.
The stored snapshot also records added/changed/deleted files for
future incremental row updates.
Preview folders are intentionally not part of the normal asset refresh
signature. Use `Refresh previews` when preview files were added, regenerated, or
became stale.

Rebuild tries to replace the generated SQLite file for speed. On Windows, if the
site or another tool has the DB open, deletion may be denied; rebuild then falls
back to clearing and rewriting the existing database.

For rebuild profiling:

```powershell
$env:AI_STUDIO_ASSET_INDEX_PROFILE='1'
node ai_studio/assets/storage/search.mjs --query crate
Remove-Item Env:\AI_STUDIO_ASSET_INDEX_PROFILE
```

## Unregistered Files

All source types use the same visibility rule: registered metadata records are
merged with discovered asset files. A discovered file that is not covered by
`assets.jsonl` remains visible in the index as:

```text
origin: unregistered
license: unknown
tags: [unregistered]
```

This prevents local game/template/global-library files from disappearing just
because they were not added to metadata yet.

The unregistered-file scan skips directories already covered by metadata before
building raw records. That keeps large libraries from paying per-file work for
assets already described by pack manifests.

## Attribution Fields

The index carries `license_url`, `attribution_required`, `author_vendor`, and
`source_page` through to Asset Viewer. CC-BY assets are shown as `credit
required`; missing author/source metadata is caught by `../license/`.

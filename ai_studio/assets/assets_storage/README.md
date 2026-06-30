# Assets Storage

Reviewed storage layer for AI Studio assets.

## Role

Assets Storage owns asset source locations, catalog records, derived preview
artifacts, the local query index, and storage-facing preparation tools. It does
not own the browser gallery UI.

## Parts

- `source_registry/`: explicit local source registry. It records game asset
  folders that Asset Viewer can display.
- `pack_manifest/`: new editable pack format for reviewed asset sources:
  `packs/<pack-id>/pack.json` plus `assets.jsonl`. This is the preferred
  canonical format for new template/game/local packs.
- `okf_catalog/`: Markdown/OKF-style shared library reader and source-first
  search entrypoint. This remains as a compatibility reader while the shared
  library is migrated pack by pack.
- `asset_index/`: local SQLite indexes, one per source, built from OKF records
  pack manifests, or folder scans for fast page, search, filter, pack, and model
  lookups.
- `search_assets.mjs`: agent-facing search command over generated SQLite
  indexes.
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

## Target Shape

Canonical asset metadata should be easy for an agent or human to add and review:

```text
<asset-source>/
  packs/
    <pack-id>/
      pack.json
      assets.jsonl
      README.md
      files/
      previews/
      licenses/
```

`pack.json` describes shared pack metadata: title, source, origin, license,
tags, genre, style, and description. `assets.jsonl` stores one JSON asset record
per line with `asset_id`, title, kind, resource path, preview path, tags, and
optional model/material fields.

Manifest and OKF metadata do not hide files that are missing from metadata. All
asset sources use the same discovery rule:

```text
registered metadata records
  + discovered asset files not covered by metadata
  = indexed assets
```

Discovered files without metadata are indexed as `origin: unregistered`,
`license: unknown`, and tagged `unregistered`. This keeps forgotten local files
visible in Asset Viewer and agent search instead of losing them during refactor
or game work.

Generated fast data stays outside git:

```text
tmp/ai_studio/assets/source_snapshots/<source-id>.sqlite
tmp/ai_studio/assets/asset_index/<source-id>.sqlite
tmp/ai_studio/assets/previews/<source-id>/
```

The current implemented query layer is `asset_index/`. The next storage slice is
`source_snapshots/`: incremental added/changed/deleted detection so refresh does
not need full rebuilds for large 100k+ asset libraries.

## Agent Search

Agents should search reusable assets through the generated index:

```powershell
node ai_studio/assets/assets_storage/search_assets.mjs --query "wooden crate" --kind model
node ai_studio/assets/assets_storage/search_assets.mjs --source-path template/assets --mode scan --tags ui,button
node ai_studio/assets/assets_storage/search_assets.mjs --query "metal floor" --license CC0 --json
```

The command refreshes the selected source index first. On unchanged sources this
is a cheap signature check; on changed sources it rebuilds the generated index
from OKF records, pack manifests, or the raw folder scanner.

# Pack Manifest

Editable pack format for AI Studio asset sources.

## Role

Pack Manifest is the preferred canonical metadata format for new reviewed asset
packs. It keeps asset metadata simple to add, review, diff, and merge, while
`../asset_index/` builds generated SQLite search indexes from it.

This module does not own binary conversion, preview rendering, or browser UI.

## Layout

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

`pack.json` describes metadata shared by all records in the pack:

```json
{
  "pack": "starter-props",
  "title": "Starter Props",
  "source": "local",
  "kind": "model",
  "origin": "mine",
  "license": "CC0",
  "genre": ["prototype"],
  "style": ["low-poly"],
  "tags": ["props", "starter"],
  "description": "Small reusable starter props."
}
```

`assets.jsonl` stores one asset per line:

```jsonl
{"asset_id":"starter__crate__cc0","title":"Crate","description":"Reusable wooden crate.","kind":"model","resource":"files/crate.glb","preview":"previews/crate.webp","tags":["crate","wood"]}
```

Paths in `assets.jsonl` are relative to the pack directory. They must not escape
the pack directory. Pack metadata is inherited by each asset and asset-level
fields may refine it.

## Current Use

`../asset_index/` automatically uses Pack Manifest for folder-backed sources
when the source root contains a `packs/` directory. Sources without `packs/` keep
using the raw folder scanner. The shared OKF Markdown library remains supported
through `../okf_catalog/` until it is migrated pack by pack.

Pack Manifest is metadata, not a visibility filter. Files with supported asset
extensions that are present under the source but not covered by `assets.jsonl`
are still indexed as `origin: unregistered`, `license: unknown`, and tagged
`unregistered`. This makes forgotten files visible so they can be accepted,
documented, moved, or deleted intentionally.

## OKF Pack Export

Use the exporter when a reviewed legacy OKF pack is ready to become a
self-contained Pack Manifest source:

```powershell
node ai_studio/assets/assets_storage/pack_manifest/export_okf_pack.mjs --pack animated-enemies --out tmp/ai_studio/pack_manifest_exports
```

The exporter writes `pack.json`, `assets.jsonl`, copied asset files, and copied
previews under `<out>/packs/<pack-id>/`. The output can be opened by Asset Index
or Asset Viewer like any other manifest-backed source.

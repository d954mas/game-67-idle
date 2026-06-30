# Pack Manifest

Editable pack format for AI Studio asset sources.

## Role

Pack Manifest is the preferred canonical metadata format for new reviewed asset
packs. It keeps asset metadata simple to add, review, diff, and merge, while
`../index/` builds generated SQLite search indexes from it.

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
  restricted/
    packs/
      <pack-id>/
        pack.json
        assets.jsonl
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
  "license_kind": "cc",
  "source_page": "https://example.com/pack",
  "author_vendor": "Example Artist",
  "attribution_required": "false",
  "notice_required": "false",
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

Template and game-local sources may also describe files that already live in the
source root because game code loads them from stable local paths. Use explicit
source-root fields instead of escaping the pack directory:

```jsonl
{"asset_id":"template__button__cc0","kind":"ui","source_resource":"ui/button.png","source_preview":"ui/button.png"}
```

`source_resource`, `source_preview`, and `source_model` are relative to the asset
source root, not the pack directory. Use them only when the file intentionally
stays in a game/template path such as `ui/`, `meshes/`, `audio/`, or `fonts/`.

For CC-BY or any `attribution_required: true` asset, `credit_text` or
`author_vendor` plus `source_page` are required before the asset is considered
ready for project use. For notice-bearing licenses such as OFL, keep
`notice_required: true` plus author/vendor and license evidence. Custom licenses
default to restricted until the record explicitly proves publish rights.

## Current Use

`../index/` automatically uses Pack Manifest for folder-backed sources
when the source root contains a `packs/` or `restricted/packs/` directory.
Sources without manifests use the raw folder scanner.

Pack Manifest is metadata, not a visibility filter. Files with supported asset
extensions that are present under the source but not covered by `assets.jsonl`
are still indexed as `origin: unregistered`, `license: unknown`, and tagged
`unregistered`. This makes forgotten files visible so they can be accepted,
documented, moved, or deleted intentionally.

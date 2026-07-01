# Asset Storage

Reviewed storage layer for AI Studio asset sources.

## Role

Asset Storage owns source registration, editable asset metadata, generated
search indexes, file-change snapshots, and derived preview cache. It does not
own browser UI; `../viewer/` displays this data.

## Structure

- `sources/`: explicit source lists for global libraries, templates, and games.
- `intake/`: stage, accept, and reject new candidate assets into Pack Manifest
  layout.
- `manifests/`: canonical editable pack format, `pack.json` plus `assets.jsonl`.
- `index/`: generated SQLite query layer, one database per source.
- `snapshots/`: generated change signatures used to skip unchanged rebuilds.
- `previews/`: generated preview preparation and cache refresh.
- `license/`: public-repo license guard and publishability policy.
- `search.mjs`: agent-facing asset search over the generated index.
- `kinds.mjs`: shared asset-kind storage constants.

## Source Shape

All reviewed sources should move toward the same layout:

```text
<asset-source>/
  asset_source.json
  _incoming/
  _accepted/
  _rejected/
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
        licenses/
```

`asset_source.json` describes the whole source: stable id, title, source type
(`global`, `template`, or `game`), owner/status, and schema version.

`pack.json` describes pack-level metadata: title, source, origin, license, tags,
genre, style, and description.

`assets.jsonl` stores one asset per line with `asset_id`, title, kind, resource
path, preview path, tags, and optional model/material fields.

Markdown is for human notes only. It is not the future source of truth.

## Visibility Rule

Metadata must not hide real files:

```text
registered metadata records
  + discovered asset files not covered by metadata
  = indexed assets
```

Discovered files without metadata are indexed as `origin: unregistered`,
`license: unknown`, and tagged `unregistered`.

A described restricted asset is valid when its metadata says it is not
publishable and its binary lives under gitignored `assets/restricted/`. It
should be visible as restricted/local data, not as an unregistered error.

## Intake

Use intake when adding new assets from local files, folders, archives, generated
outputs, or sourced downloads:

```powershell
node ai_studio/assets/storage/intake/stage.mjs --source-root <asset-source> --input <file-or-folder> --source <source> --slug <candidate>
node ai_studio/assets/storage/intake/accept.mjs --source-root <asset-source> --source <source> --slug <candidate> --file <relative-file> --pack <pack-id> --asset-id <id> --kind <kind> --license <license>
node ai_studio/assets/storage/intake/reject.mjs --source-root <asset-source> --source <source> --slug <candidate>
```

`accept.mjs` writes Pack Manifest metadata. Publishable assets go under
`packs/`; non-publishable assets go under `restricted/packs/` so they remain
visible to Asset Viewer while their binaries stay out of public git. Accepted
candidate folders move from `_incoming/` to `_accepted/` as audit trail; rejected
candidates move to `_rejected/`.

When accepting, record known license/provenance options: `--license-url`,
`--source-page-url`, `--author-vendor`, and `--license-kind`. For
custom/private/unknown licenses, also pass explicit rights flags when known:
`--commercial-use`, `--modification-allowed`, `--redistribution-allowed`, and
`--publish`. CC-BY and notice-bearing assets may enter development without final
credit text, but keep `--attribution-required`, `--notice-required`, source
page, and author/vendor metadata so release validation can find the remaining
credit/notice work.

## License Guard

This repository is public. Private, paid, unknown-license, and
non-redistributable asset binaries must not enter git.

```powershell
node ai_studio/assets/storage/license/restricted_assets_guard.mjs
node --test ai_studio/assets/storage/license/restricted_assets_guard.test.mjs
```

The guard fails closed: unknown license is treated as not publishable. Put
restricted binaries under gitignored `assets/restricted/` and commit only the
metadata/license record.

## Generated Data

Generated data stays outside git:

```text
tmp/ai_studio/assets/index/<source-id>.sqlite
tmp/ai_studio/assets/snapshots/<source-id>.json
tmp/ai_studio/assets/previews/<source-id>/
```

Refresh is explicit. No filesystem watch mode belongs here.

## Agent Search

```powershell
node ai_studio/assets/storage/search.mjs --query "wooden crate" --kind model
node ai_studio/assets/storage/search.mjs --source-path templates/template/assets --source-type local --tags ui,button
node ai_studio/assets/storage/search.mjs --query "metal floor" --license CC0 --json
```

`search.mjs` defaults to active `global-library` from
`sources/libraries.json` when one is registered and reads the generated index by
default. If that source is disabled, it falls back to another active library.
Add `--refresh` only when local files or manifests changed and the query must
update the index first. Unchanged sources use a snapshot check; changed sources
rebuild from Pack Manifest metadata or a raw folder scan.

# Asset Intake

Asset Intake moves candidate files into the reviewed Asset Storage layout.

## Role

Intake is the front door for new local, sourced, generated, or paid assets. It
does not render previews, build engine integration artifacts, or decide final release credits.
It records provenance, applies the license decision, and writes Pack Manifest
metadata that `../index/` can read.

## Flow

```text
stage  ->  review  ->  accept | reject  ->  refresh index/previews
```

- `stage.mjs`: copy a file/folder into `_incoming/<source>/<slug>/` with hashes.
- `accept.mjs`: move one staged file into `packs/<pack-id>/` or
  `restricted/packs/<pack-id>/`, then append/update `assets.jsonl`.
- `reject.mjs`: move a staged candidate into `_rejected/` or delete it.

## Source Layout

```text
<asset-source>/
  _incoming/
  _rejected/
  packs/
    <pack-id>/
      pack.json
      assets.jsonl
      files/
      licenses/
  restricted/
    packs/
      <pack-id>/
        pack.json
        assets.jsonl
        files/
        licenses/
```

Publishable assets go under `packs/`. Paid, private, unknown-license, custom
non-publishable, and non-redistributable binaries go under `restricted/packs/`.
Restricted metadata remains visible to the index, but binaries stay under a
gitignored root for public game/template sources.

## Commands

```powershell
node ai_studio/assets/storage/intake/stage.mjs --source-root <asset-source> --input <file-or-folder> --source <site-or-owner> --slug <candidate>
node ai_studio/assets/storage/intake/accept.mjs --source-root <asset-source> --source <site-or-owner> --slug <candidate> --file <relative-file> --pack <pack-id> --asset-id <id> --kind <kind> --license <license> --license-url <url> --title <title> --tags <a,b>
node ai_studio/assets/storage/intake/reject.mjs --source-root <asset-source> --source <site-or-owner> --slug <candidate>
```

CC-BY attribution and notice metadata may be completed later during development,
but `attribution_required` and `notice_required` must be recorded so release
validation can find the debt.

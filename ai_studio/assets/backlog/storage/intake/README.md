# ssset Intake

ssset Intake moves candidate files into the reviewed ssset Storage layout.

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
  `restricted/packs/<pack-id>/`, append/update `assets.jsonl`, then move the
  staged candidate folder to `_accepted/<source>/<slug>/` as an audit trail.
- `reject.mjs`: move a staged candidate into `_rejected/` or delete it.

`_incoming/` means "not decided yet". ssset Index may show those files as
`unregistered` so the user can see forgotten local additions. `_accepted/` and
`_rejected/` are lifecycle/audit folders, not asset sources, and are not shown
as unregistered assets.

For storage roots with retained intake audit folders, ssset Index also
suppresses `_incoming` files whose `intake.json` hash already matches a Pack
Manifest record. This keeps accepted candidates from appearing twice without
deleting external files.

## Source Layout

```text
<asset-source>/
  _incoming/
  _accepted/
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
node ai_studio/assets/backlog/storage/intake/stage.mjs --source-root <asset-source> --input <file-or-folder> --source <site-or-owner> --slug <candidate>
node ai_studio/assets/backlog/storage/intake/accept.mjs --source-root <asset-source> --source <site-or-owner> --slug <candidate> --file <relative-file> --pack <pack-id> --asset-id <id> --kind <kind> --license <license>
node ai_studio/assets/backlog/storage/intake/reject.mjs --source-root <asset-source> --source <site-or-owner> --slug <candidate>
```

When accepting, also record known metadata with `--title`, `--tags`,
`--license-url`, `--source-page-url`, `--author-vendor`, and `--license-kind`.
For custom/private/unknown licenses, pass explicit rights flags when known:
`--commercial-use`, `--modification-allowed`, `--redistribution-allowed`, and
`--publish`. CC-BY attribution and notice metadata may be completed later during
development, but source page, author/vendor, `--attribution-required`, and
`--notice-required` should be recorded so release validation can find the debt.

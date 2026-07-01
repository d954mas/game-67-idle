# Restricted Assets

Load this before handling paid, account-gated, unknown-license, or
non-redistributable assets.

## Rule

Partition by publishability:

- **Publishable**: CC0/OFL/compatible assets may have binaries committed when the
  manifest/license record proves redistribution is allowed.
- **Restricted**: paid/commercial/unknown assets keep binaries out of git. Only
  metadata, license notes, and reconstruction instructions may be committed.

The shared decision registry and guard live in
`ai_studio/assets/backlog/storage/license/`.

```powershell
node ai_studio/assets/backlog/storage/license/restricted_assets_guard.mjs
```

## Manual Intake

Paid assets are usually account-gated. Record the product/source page and local
file hash; do not store private download links.

```powershell
node ai_studio/assets/backlog/storage/intake/stage.mjs --manual --input "<local-file>" --source <source> --slug <slug> --license "<license>" --source-page-url <page-url> --publish false --source-root <asset-source>
```

After preparation, accept with `--publish false` or the preserved intake flag.

## Reuse In A Game

```powershell
node ai_studio/assets/viewer/pull.mjs --ids <asset-id> --to <game>/assets --apply
```

Expected result:

- binary under gitignored `assets/restricted/...`;
- manifest/license metadata under committed game asset metadata;
- pack/build code points at the local restricted path only when the file exists.

If the asset is described and the binary is missing from git because it is
restricted/local, that is not an error. It is an error only when a restricted or
unknown-license binary is tracked in git, or when a committed binary has no
publishable metadata.

CC-BY is publishable but attribution-bearing. It may be used during development
without final credit metadata, but release validation must fail until the asset
metadata records `credit_text` or author/credit plus source page. OFL and
similar notice-bearing licenses are also allowed during development, but release
validation needs the license URL/file and author/vendor for third-party notices.

Custom licenses are restricted by default. Mark `publish: true` only with
explicit `redistribution_allowed: true`, `commercial_use: true`,
`modification_allowed: true`, and license evidence.

If the guard fails, either record the binary in a manifest with a publishable
license or move the binary to the restricted path and untrack it.

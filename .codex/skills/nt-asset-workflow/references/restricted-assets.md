# Restricted Assets

Load this before handling paid, account-gated, unknown-license, or
non-redistributable assets.

## Rule

Partition by publishability:

- **Publishable**: CC0/OFL/compatible assets may have binaries committed when the
  manifest/license record proves redistribution is allowed.
- **Restricted**: paid/commercial/unknown assets keep binaries out of git. Only
  metadata, license notes, and reconstruction instructions may be committed.

The Studio exception is an explicit lead grant recorded in a private game's
`assets/PURCHASED_EXPORTS.md`: only that game's shipped, game-ready purchased
exports may be tracked there. Source archives and unused pack content stay out
of every repository. This does not make the source pack publicly redistributable
or authorize promotion through a public-license-only tool.

The shared decision registry and guard live in
`ai_studio/assets/licenses/`.

```powershell
node ai_studio/assets/licenses/restricted_assets_guard.mjs
```

## Manual Intake

Paid assets are usually account-gated. Record the product/source page and local
file hash; do not store private download links.

```powershell
node ai_studio/assets/intake/stage.mjs --manual --input "<local-file>" --source <source> --slug <slug> --license "<license>" --source-page-url <page-url> --publish false --source-root <asset-source>
```

After preparation, accept with `--publish false` or the preserved intake flag.

## Reuse In A Game

```powershell
node ai_studio/assets/gallery/pull.mjs --ids <asset-id> --to <game>/assets --apply
```

Expected result:

- binary under gitignored `assets/restricted/...`;
- manifest/license metadata under committed game asset metadata;
- pack/build code points at the local restricted path only when the file exists.

An intentionally local restricted binary may be absent from git. Tracking a
restricted binary requires the narrow private-game export grant above; every
committed asset still needs license, provenance, integrity and `origin`.

CC-BY is publishable but attribution-bearing. It may be used during development
without final credit metadata, but release validation must fail until the asset
metadata records `credit_text` or author/credit plus source page. OFL and
similar notice-bearing licenses are also allowed during development, but release
validation needs the license URL/file and author/vendor for third-party notices.

Custom licenses are restricted by default. Mark `publish: true` only with
explicit `redistribution_allowed: true`, `commercial_use: true`,
`modification_allowed: true`, and license evidence.

If the guard fails, resolve the license/ownership finding against its contract.
Do not relabel a restricted export as public or bypass the guard. Report a
missing supported path when the private-game grant cannot be represented.

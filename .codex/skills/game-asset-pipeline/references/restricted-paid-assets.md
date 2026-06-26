# Restricted (Paid / Licensed) Assets

Load this when bringing in assets whose license forbids redistribution in a
public repo — paid marketplace packs (CGTrader subscription, etc.), commercial
music, or licensed fonts. The repos here are open, and future game repos may be
open too, so paid binaries must never enter git.

## The Rule

Partition assets by **publishability** (the right to redistribute), not by source
or type:

- **Publishable** (CC0/OFL/CC-BY ...): binary committed under `assets/source|previews/`.
- **Restricted** (paid/commercial/unknown): binary lives only under the gitignored
  `assets/restricted/` root and in the private library. Only the catalog `.md` +
  license `.md` (metadata, no binary) are committed — they are the reconstruction
  recipe (re-pull from the library).

Every committed asset MUST have a catalog record with a recorded license. The leak
guard `tools/assets/audit/restricted_assets_guard.mjs` (wired into
`node ai_studio/core_harness/validation/pipeline_validate.mjs`) blocks any tracked binary that has no catalog or a
non-publishable license — this catches both a pulled paid asset and one dropped in
by hand. Publishability is decided by `tools/assets/restricted.mjs`
(`publish` field → `redistribution_allowed` → license string; unknown = NOT
publishable).

## Manual Paid Intake (no download link is stored)

Paid packs are downloaded by hand from an account-gated page, so no shareable URL
exists. Drop the downloaded file in, recording only the product page + sha256:

```powershell
node tools/assets/intake/download_source_asset.mjs --manual `
  --url "C:\Users\...\Downloads\uploads_files_..._NatureGradientPack.zip" `
  --source cgtrader --slug nature-gradient-pack `
  --license-name "CGTrader Royalty Free" `
  --source-page-url https://www.cgtrader.com/3d-models/<product> `
  --publish false --library <library-root>
```

`--manual` reads the local file but does NOT store its path; `intake.json` keeps
the product page, original filename, bytes, sha256, `manual: true`, `publish: false`.

## Parse the Pack ("разобрать ассет") → engine-ready glb

A marketplace pack ships several formats + textures (the NatureGradientPack example
holds `.fbx`, `.mb`, `.blend`, `.obj`+`.mtl`, `Texture/`). Prepare it like any
sourced model (see `game-asset-prep` and `game-3d-models`):

1. Unzip into `_incoming/<source>/<slug>/`.
2. Inventory the formats; pick one clean source (prefer `.obj`/`.glb`/`.fbx`).
3. Convert to `.glb`, split by material; keep only the textures actually used.
4. Dedup shared textures; render a preview.
5. Accept into the library with `--publish false`:

```powershell
node tools/assets/intake/accept_incoming_asset.mjs --source cgtrader `
  --slug nature-gradient-pack --asset-id cgtrader__nature-gradient-pack__royalty-free `
  --kind model --title "Nature Gradient Pack" --description "<searchable sentence>" `
  --license-name "CGTrader Royalty Free" --license-url <terms-url> `
  --tags "nature,foliage,paid,cgtrader" --origin sourced --library <library-root>
```

`accept` reads `publish` from `intake.json` (or pass `--publish false`); it writes
`publish:` + `redistribution_allowed:` into the catalog and the license note, and
records the download as "manual/account-gated — not stored".

## Reuse in a Game

`pull.mjs` routes a non-publishable asset automatically:

```powershell
node ai_studio/assets/asset_viewer/pull.mjs --ids cgtrader__nature-gradient-pack__royalty-free --to assets --apply
```

- binary → `assets/restricted/source/models/<asset-id>/` (gitignored)
- catalog `.md` (with `publish: false`, `resource: restricted/source/...`) →
  `assets/catalog/...` (committed)
- license `.md` → `assets/licenses/...` (committed)

The builder uses explicit paths in `build_packs.c`, so point it at the
`assets/restricted/...` path — the file only needs to exist locally at build time.

## If the Guard Fails

`error: ... license is not publishable` / `no catalog record` means a paid or
unlicensed binary is tracked. Fix by either cataloging it with a publishable
license, or moving the binary under `assets/restricted/` (gitignored) and re-pull.
`git rm --cached <path>` to unstage a binary that was already added.

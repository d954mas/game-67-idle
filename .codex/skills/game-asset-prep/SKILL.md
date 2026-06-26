---
name: game-asset-prep
description: "Use to PREPARE a sourced or made asset into engine-ready form AND catalog it in the shared library for reuse: convert models obj/fbx->glb (split by material), dedup shared textures, generate previews, record origin/license, then find+copy into the next game. Reuse lifecycle: source -> prepare -> library -> reuse."
---

# Game Asset Prep (reuse across games)

Turn a raw asset into something the NEXT game drops in with one search. Prepare
ONCE, store in the shared library, reuse many — do not re-prepare per game.

## Lifecycle

1. **Source first** — `node tools/assets/source/find_assets.mjs --kind <k> --genre <g> --tags <t>`
   (also `--query`, `--json`): reuse a library hit (3700+ ready glb), else search
   free CC0/OFL sources, else generate.
2. **Prepare** (engine-ready, per type) — see below.
3. **Transfer to library** — catalog with origin + license + preview.
4. **Reuse** — `node ai_studio/assets/asset_viewer/pull.mjs --ids <library_id> --to assets --apply`
   (omit `--apply` to dry-run): copies files + writes a game OKF record with
   `source_id` (linked; library stays canonical). No `source_id` = new/game-local
   — `promote` it back to the library.

## Prepare by type

- **Models** (obj/fbx -> glb): `tools/assets/obj_to_glb.py` (Blender;
  split-by-material — engine reads one material/mesh). PREFER a vendor-shipped glb
  if it exists (obj winding can break normals). Reuse one handle/part. game-3d-models.
- **Textures**: dedup a pack's SHARED atlas/colormap — store it ONCE, never a copy
  per model; run the tileable audit. See game-texture-generation.
- **UI kits**: cut / slice9 / manifests. See generated-game-ui-assets.
- **Fonts**: TTF/OTF (OFL); kind `font`.

## Transfer to library

Catalog each reusable asset with provenance:
- downloaded (single URL/file): `tools/assets/intake/download_source_asset.mjs` then
  `tools/assets/intake/accept_incoming_asset.mjs`.
- a ZIP or folder (bulk pack, no direct URL): `tools/assets/intake/ingest_archive.mjs`
  stages it into `_incoming/<source>/<slug>/` with per-file sha256, then `accept`.
- paid pack (CGTrader etc.): manual intake `--manual --publish false`; binary
  routes to the gitignored `assets/restricted/`, only the catalog `.md` is
  committed. See game-asset-pipeline `references/restricted-paid-assets.md`.
- project-vendored / freshly prepared: `node ai_studio/assets/asset_viewer/promote.mjs --ids
  ... --source <s> --license <L> --origin <mine|ai|sourced> [--pack <slug>]`.
A kit -> `--pack`: groups under `catalog/<kind>/<pack>/` + `_pack.md` + one shared license.
Record `origin` + license + preview; id = `<source>__<slug>__<license>`.
Catalog reusable assets BEFORE copying project-local (see game-asset-pipeline).

## Review / share

`build_review.mjs` — library/scan/review browser (facets, search, 3D-in-modal);
cards show `linked` vs `new`. Big library: `--ref` + `serve_gallery.mjs`. Phone: `serve_tunnel.mjs`.

## Rules

- One prepared, engine-ready copy in the library; projects copy FROM it (never
  load the library directly). Shared textures: ONE copy, never per-model.
- Record origin + license + provenance before a reusable asset is accepted.

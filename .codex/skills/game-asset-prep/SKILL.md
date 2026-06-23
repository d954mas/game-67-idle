---
name: game-asset-prep
description: "Use to PREPARE a sourced or made asset into engine-ready form AND catalog it in the shared library so future games reuse it easily: convert models obj/fbx->glb (split by material), dedup shared textures, generate previews, record origin/license, then find+copy into the next game. The reuse lifecycle across games: source -> prepare -> library -> reuse."
---

# Game Asset Prep (reuse across games)

Turn a raw asset into something the NEXT game drops in with one search. Prepare
ONCE, store in the shared library, reuse many — do not re-prepare per game.

## Lifecycle

1. **Source first** — `node tools/assets/source/find_assets.mjs --kind <k> --tags ...`:
   reuse a library hit, else search free CC0/OFL sources, else generate.
2. **Prepare** (engine-ready, per type) — see below.
3. **Transfer to library** — catalog with origin + license + preview.
4. **Reuse** — `pull.mjs --ids <library_id> --to <game>/assets`: copies files +
   writes a game OKF record with `source_id` (linked; library stays canonical).
   No `source_id` = new/game-local — `promote` it to the library.

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
- downloaded: `tools/assets/intake/download_source_asset.mjs` then
  `tools/assets/intake/accept_incoming_asset.mjs`.
- project-vendored / freshly prepared: `node tools/asset_review/promote.mjs --ids
  ... --source <s> --license <L> --origin <mine|ai|sourced> [--pack <slug>]`.
A kit -> `--pack`: groups under `catalog/<kind>/<pack>/` + `_pack.md` + one shared license.
Record `origin` + license + preview; id = `<source>__<slug>__<license>`.
Catalog reusable assets BEFORE copying project-local (see game-asset-pipeline).

## Review / share

`build_review.mjs` — packs/bundles browser (cover montage, genre/tags, facets,
search, 3D-in-modal): `--mode library` · `--mode scan --path <dir>` · `--mode
review --game <id>`. Cards show `linked` vs `new`. `serve_tunnel.mjs` → phone.

## Rules

- One prepared, engine-ready copy lives in the library; projects copy FROM it,
  never load from the library directly.
- Shared textures: ONE copy in the library, never duplicated per model.
- Always record origin + license + provenance before a reusable asset is accepted.

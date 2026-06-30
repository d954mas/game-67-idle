---
name: game-asset-pipeline
description: "Use when sourcing, adding, converting, packing, referencing, validating, or organizing game assets: textures, sprites, models, fonts, audio, atlases, asset headers, manifests, bundles, pack builders, raw/final asset locations, missing resources, or runtime asset loading failures. Source first — search the shared library + free CC0/OFL sources before generating; catalog reusable assets with origin (mine|ai|sourced). Owns runtime asset tracing + sourcing/library; UI-kit cutting goes to generated-game-ui-assets, raster generation to delegated-image-generation."
---

# Game Asset Pipeline

Keep source assets, generated outputs, and runtime packs traceable. Pair with
`generated-game-ui-assets` for reusable runtime UI kits.

## Load Only What Applies

- `references/asset-source-and-cutout-rules.md`: source of truth, art job,
  generated provenance, `new_art_job.mjs`, `new_generation_record.mjs`,
  non-empty workflow JSON, no-seed reason, `chroma_key_alpha.py`, slice9
  margins, and `--final-art`.
- `references/shared-free-asset-library.md`: shared downloaded/free source
  assets as OKF Markdown records, `download_source_asset.mjs`,
  `accept_incoming_asset.mjs`, license/integrity, `preview_2x2`, `seam_audit`,
  import boundary.
- `references/restricted-paid-assets.md`: paid/licensed assets — manual intake (no
  download link), `publish:false`, routing to gitignored `assets/restricted/`, guard.
- `references/pack-builder-rules.md`: pack builders, measured failure,
  smallest pack build, project-relative outputs, atomic writes, review atlases,
  and Fail loudly behavior.
- `generated-game-ui-assets`: reusable UI-asset gate sequence and Report Shape.

## Minimal Workflow

1. Identify source of truth and generated/runtime outputs.
2. Source before you generate: run `node ai_studio/assets/assets_storage/okf_catalog/find_assets.mjs`
   to search the shared library by tags/resource, then the printed free CC0/OFL
   sources; record license/integrity/origin.
3. For generated multi-asset work, create the art job before crop/pack ids.
4. Record accepted provenance: real workflow data, non-empty workflow JSON, or
   explicit no-seed reason.
5. Read pack/build scripts first; require measured failure before bypassing.
6. Add the smallest runtime proof, regenerate packs, verify loading.
7. Before final art claims, run the final-art gate.

## Always-On Rules

- Keep raw source, shared downloaded assets, and generated runtime assets separate.
- Catalog reusable free/CC0 assets in the library (record + license + provenance +
  `origin`) before copying project-local, so the next source-first search finds them.
- Every committed asset needs a recorded license; paid/non-redistributable assets
  go to gitignored `assets/restricted/` (only the catalog `.md` committed); the
  guard blocks paid binaries. See `references/restricted-paid-assets.md`.
- Project code uses project-local copies, not shared library or scratch paths.
- Generated headers/packs must be reproducible from source plus builder code.
- Crop rectangles, pivots, trim rules, and slice9 margins belong in manifests.
- Pack builders/audits must Fail loudly and use project-relative paths.

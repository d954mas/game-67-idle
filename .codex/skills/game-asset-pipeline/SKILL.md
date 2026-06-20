---
name: game-asset-pipeline
description: Use when adding, converting, packing, referencing, validating, or organizing game assets: textures, sprites, models, fonts, audio, atlases, generated asset headers, manifests, bundles, engine-specific asset packs, pack builders, raw/final asset locations, generated code, missing resources, or runtime asset loading failures. Owns packing/tracing assets into the runtime; UI-kit cutting/manifests go to generated-game-ui-assets, raster generation to delegated-image-generation.
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
  assets as OKF Markdown records, intake helper, license/integrity, import boundary.
- `references/pack-builder-rules.md`: pack builders, measured failure,
  smallest pack build, project-relative outputs, atomic writes, review atlases,
  and Fail loudly behavior.
- `generated-game-ui-assets`: reusable UI-asset gate sequence and Report Shape.

## Minimal Workflow

1. Identify source of truth and generated/runtime outputs.
2. For downloaded/free assets, search Markdown records by tags/resource; record
   license plus integrity before integration.
3. For generated multi-asset work, create the art job before crop/pack ids.
4. Record accepted provenance: real workflow data, non-empty workflow JSON, or
   explicit no-seed reason.
5. Read pack/build scripts first; require measured failure before bypassing.
6. Add the smallest runtime proof, regenerate packs, verify loading.
7. Before final generated/artist art claims, run the final-art gate.

## Always-On Rules

- Keep raw source, shared downloaded assets, and generated runtime assets separate.
- Project code uses project-local copies, not shared library or scratch paths.
- Generated headers/packs must be reproducible from source plus builder code.
- Crop rectangles, pivots, trim rules, and slice9 margins belong in manifests.
- Pack builders/audits must Fail loudly and use project-relative paths.

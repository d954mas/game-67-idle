---
name: game-asset-pipeline
description: Use when adding, converting, packing, referencing, validating, or organizing game assets: textures, sprites, models, fonts, audio, atlases, generated asset headers, manifests, bundles, engine-specific asset packs, pack builders, raw/final asset locations, generated code, missing resources, or runtime asset loading failures. Owns packing/tracing assets into the runtime; UI-kit cutting/manifests go to generated-game-ui-assets, raster generation to delegated-image-generation.
---

# Game Asset Pipeline

Use this skill to keep source assets, generated outputs, and runtime packs
traceable and reproducible. Pair it with `.codex/skills/generated-game-ui-assets/`
for reusable generated runtime UI kits.

## Load Only What Applies

- `references/asset-source-and-cutout-rules.md`: source of truth, generated
  provenance, art job, `new_art_job.mjs`, `new_generation_record.mjs`,
  non-empty workflow JSON, no-seed reason, `chroma_key_alpha.py`, slice9 margins,
  and `--final-art`.
- `references/shared-free-asset-library.md`: shared downloaded/free source
  assets as OKF Markdown records, intake helper, license/integrity checks, and
  import boundary.
- `references/pack-builder-rules.md`: pack builders, measured failure,
  smallest pack build, project-relative outputs, atomic writes, review atlases,
  and Fail loudly behavior.
- `.codex/skills/generated-game-ui-assets/`: reusable UI-asset gate sequence and
  Report Shape.

## Minimal Workflow

1. Identify source of truth and generated/runtime outputs.
2. For downloaded/free assets, search shared Markdown records by tags/resource,
   then record license plus integrity before project integration.
3. For generated multi-asset work, identify or create the art job before crop
   coordinates or pack ids.
4. Record accepted source provenance with real workflow data, non-empty workflow
   JSON, or an explicit no-seed reason.
5. Read pack/build scripts before adding logic; do not assume the pack/material
   path is too slow without measured failure.
6. Add the smallest asset path proving runtime integration, regenerate packs,
   and verify generated files/runtime loading.
7. Before final generated/artist art claims, run the art-job final-art gate.

## Always-On Rules

- Keep raw source assets separate from generated runtime assets.
- Shared downloaded assets live outside projects until selected; project code
  uses project-local copies, not the shared library path.
- Do not reference scratch/temp files as final game assets.
- Generated headers and packs must be reproducible from source plus builder code.
- Crop rectangles, pivots, trim rules, and slice9 margins belong in manifests.
- Pack builders/audits must Fail loudly and use project-relative paths.

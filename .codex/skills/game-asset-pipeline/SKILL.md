---
name: game-asset-pipeline
description: Use when adding, converting, packing, referencing, validating, or organizing game assets: textures, sprites, models, fonts, audio, atlases, generated asset headers, manifests, bundles, engine-specific asset packs, pack builders, raw/final asset locations, generated code, missing resources, or runtime asset loading failures.
---

# Game Asset Pipeline

Use this skill to keep source assets, generated outputs, and runtime packs
traceable and reproducible. For reusable generated runtime UI kits, also use
`.codex/skills/generated-game-ui-assets/`.

## Load Only What Applies

- `references/asset-source-and-cutout-rules.md`: source of truth, generated
  provenance, art job, `new_art_job.mjs`, `new_generation_record.mjs`,
  non-empty workflow JSON, no-seed reason, prompt packets,
  `chroma_key_alpha.py`, crop/cutout, slice9 margins, `--final-art`.
- `references/pack-builder-rules.md`: pack builders, measured failure before
  shortcuts, smallest pack build, project-relative outputs, atomic writes,
  review atlas rules, fail loudly on missing assets.
- `.codex/skills/generated-game-ui-assets/`: ordered reusable UI-asset gate
  sequence and Report Shape.

## Minimal Workflow

1. Identify source of truth (`assets/`, `raw/`, `art/`, `gamedesign/art/`) and
   generated/runtime outputs (headers, atlases, packs, bundles, caches).
2. For generated multi-asset work, identify or create the art job before crop
   coordinates or pack ids.
3. Record accepted generated/artist source provenance; use real workflow data,
   non-empty workflow JSON, or an explicit no-seed reason.
4. Read existing pack/build scripts before adding logic.
5. Do not assume the pack/material path is too slow; inspect and run or measure
   the smallest pack build before choosing a shortcut.
6. Add the smallest asset path proving runtime integration, regenerate packs
   with the project task/preset, and verify generated files/runtime loading.
7. Before final generated/artist art claims, run the art-job final-art gate.

## Always-On Rules

- Keep raw source assets separate from generated runtime assets.
- Do not reference scratch/temp files as final game assets.
- Generated headers and packs must be reproducible from source plus builder
  code.
- Crop rectangles, pivots, trim rules, and slice9 margins belong in manifests.
- Reuse `tools/assets/chroma_key_alpha.py` for shared chroma/cutout cleanup.
- Pack builders and audits must Fail loudly on missing required source assets
  and use project-relative paths.

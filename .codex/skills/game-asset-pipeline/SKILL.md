---
name: game-asset-pipeline
description: Use when adding, converting, packing, referencing, validating, or organizing game assets such as textures, sprites, models, fonts, audio, atlases, generated asset headers, manifests, bundles, or engine-specific asset packs. Triggers include asset build scripts, pack builders, raw versus final asset locations, generated code, missing resources, and runtime asset loading failures.
---

# Game Asset Pipeline

Use this skill to keep source assets, generated outputs, and runtime packs
understandable.

For reusable generated runtime UI kits, also use
`.codex/skills/generated-game-ui-assets/`; it owns the full source sheet ->
slice9/icon -> audit -> responsive proof workflow.

## Load Only What Applies

- `references/asset-source-and-cutout-rules.md`: source of truth, generated
  provenance, art job, `new_art_job.mjs`, `new_generation_record.mjs`,
  non-empty workflow JSON, no-seed reason, prompt packets, `chroma_key_alpha.py`,
  crop/cutout, slice9 margins, `--final-art`, generated UI production gate.
- `references/pack-builder-rules.md`: pack builders, measured failure before
  shortcuts, smallest pack build, project-relative outputs, atomic writes,
  review atlas rules, fail loudly on missing assets.
- `.codex/skills/generated-game-ui-assets/`: ordered reusable UI-asset gate
  sequence and Report Shape.

## Minimal Workflow

1. Identify the asset source of truth: `assets/`, `raw/`, `art/`,
   `gamedesign/art/`, or local equivalent.
2. Identify generated/runtime outputs: pack files, generated headers, atlases,
   bundles, caches.
3. For generated multi-asset work, identify the art job/request packet. If it
   does not exist, scaffold it with `tools/assets/job/new_art_job.mjs` before
   adding crop coordinates or pack ids. For generated UI, run
   `node tools/assets/job/validate_art_job.mjs --job <art-job>` before
   generation or slicing.
4. For generated or artist source art, create provenance with
   `node tools/assets/job/new_generation_record.mjs`. Use real workflow
   provenance or non-empty workflow JSON; record no-seed reason with
   `--no-seed-reason` when needed.
5. Before image generation for a source family, compile a prompt packet from the
   art job with `node tools/assets/job/plan_source_sheet_prompt.mjs`.
6. Read the existing pack/build script before adding logic.
7. Do not assume the pack/material path is too slow; inspect and run/measure the
   smallest pack build before choosing a direct shortcut.
8. Add the smallest asset path that proves runtime integration.
9. Regenerate packs with the project task or preset.
10. Verify generated files and runtime loading behavior when possible.
11. Before claiming final generated/artist art, run:
    `node tools/assets/job/validate_art_job.mjs --job <art-job> --final-art`.

## Always-On Rules

- Keep raw source assets separate from generated runtime assets.
- Keep project-specific generated assets and ids separate from other concepts.
- Do not reference scratch/temp files as final game assets.
- Generated headers and pack files should be reproducible from source assets and
  builder code.
- Crop rectangles, pivots, trim rules, and slice9 margins belong in manifests,
  not chat or screenshots.
- Chroma/cutout cleanup should reuse `tools/assets/chroma_key_alpha.py`.
- Pack builders and audits should fail loudly on missing required source assets.
  Use project-relative paths.

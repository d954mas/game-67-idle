---
name: game-asset-pipeline
description: Use when adding, converting, packing, referencing, validating, or organizing game assets such as textures, sprites, models, fonts, audio, atlases, generated asset headers, manifests, bundles, or engine-specific asset packs. Triggers include asset build scripts, pack builders, raw versus final asset locations, generated code, missing resources, and runtime asset loading failures.
---

# Game Asset Pipeline

Use this skill to keep source assets, generated outputs, and runtime packs understandable.

## Workflow

1. Identify the asset source of truth: `assets/`, `raw/`, `art/`, `gamedesign/art/`, or local equivalent.
2. Identify generated/runtime outputs: pack files, generated headers, atlases, bundles, caches.
3. For generated multi-asset work, identify the art job/request packet. If it
   does not exist, scaffold it with `tools/assets/new_art_job.mjs` before
   adding crop coordinates or pack ids.
4. Read the existing pack/build script before adding new asset logic.
5. Do not assume the pack/material path is too slow. If the engine or project
   has pack builders and caches, inspect the builder and run or measure the
   smallest pack build before choosing a direct PNG/runtime shortcut.
6. Add the smallest asset path that proves the runtime integration.
7. Regenerate packs with the project task or preset.
8. Verify both generated files and runtime loading behavior when possible.

## Rules

- Keep raw source assets separate from generated runtime assets.
- Do not reference scratch/temp files as final game assets.
- Generated headers and pack files should be reproducible from source assets and builder code.
- If generated files are ignored by git, confirm the build task recreates them.
- If generated files are committed by project convention, keep diffs small and explain why.
- A game-local loader shortcut must be justified by a measured failure, missing
  engine capability, or explicit iteration-only boundary.
- For generated UI, keep crop rectangles, pivots, trim rules, and slice9 margins
  in a manifest; do not preserve them only in chat or screenshots.
- For generated art jobs, keep selected source sheets, rejected-output notes,
  runtime asset ids, pack commands, and screenshot evidence referenced from the
  same job contract.

## Pack Builder Changes

When editing a pack builder:

- Add resource ids with stable names.
- Keep cache directories and output paths project-relative.
- Fail loudly on missing required source assets.
- Print enough output for a user or agent to know what was generated.

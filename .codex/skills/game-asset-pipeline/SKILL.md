---
name: game-asset-pipeline
description: Use when adding, converting, packing, referencing, validating, or organizing game assets such as textures, sprites, models, fonts, audio, atlases, generated asset headers, manifests, bundles, or engine-specific asset packs. Triggers include asset build scripts, pack builders, raw versus final asset locations, generated code, missing resources, and runtime asset loading failures.
---

# Game Asset Pipeline

Use this skill to keep source assets, generated outputs, and runtime packs understandable.

## Workflow

1. Identify the asset source of truth: `assets/`, `raw/`, `art/`, `gamedesign/art/`, or local equivalent.
2. Identify generated/runtime outputs: pack files, generated headers, atlases, bundles, caches.
3. Read the existing pack/build script before adding new asset logic.
4. Add the smallest asset path that proves the runtime integration.
5. Regenerate packs with the project task or preset.
6. Verify both generated files and runtime loading behavior when possible.

## Rules

- Keep raw source assets separate from generated runtime assets.
- Do not reference scratch/temp files as final game assets.
- Generated headers and pack files should be reproducible from source assets and builder code.
- If generated files are ignored by git, confirm the build task recreates them.
- If generated files are committed by project convention, keep diffs small and explain why.

## Pack Builder Changes

When editing a pack builder:

- Add resource ids with stable names.
- Keep cache directories and output paths project-relative.
- Fail loudly on missing required source assets.
- Print enough output for a user or agent to know what was generated.


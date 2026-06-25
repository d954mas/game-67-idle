---
name: game-visual-art-direction
description: Use when defining, generating, sourcing, reviewing, integrating, or improving game visual direction, art assets, UI kits, fake shots, sprites, icon sets, generated visuals, child-friendly visual polish, release-quality presentation, or replacing placeholder/procedural visuals with real bitmap/model assets. Source first — search the shared asset library and free CC0/OFL sources before generating. Owns visual direction and quality judgment; raster generation is delegated-image-generation, reusable UI-kit cutting is generated-game-ui-assets, sourcing/library is game-asset-pipeline.
---

# Game Visual Art Direction

Use when visual quality is part of the task: generated art, sprites, UI kits,
fake shots, polished builds, or release-quality presentation.

## Load Only What Applies

- `references/visual-workflow-gates.md`: accepted target, reference
  deconstruction, screen grammar, mismatch audit, Art-First Gate, Reusable UI
  Gate, generated asset rules, report.
- `gamedesign/knowledge/reference_deconstruction.md`: named references or
  lead-rejected reference mismatch.
- `generated-game-ui-assets`: reusable runtime UI kit production.
- `game-asset-pipeline`: art jobs, manifests, provenance, packs.
- `game-runtime-automation`: native screenshot/video proof.

## Default Route

1. Load active task, accepted target, screenshot, runtime harness, and only
   relevant references.
2. Write the 5-line visual session contract: goal, non-goal, proof, stop
   condition, likely files.
3. Produce Reference Digest before final art when named refs drive the look;
   include current screenshot-vs-target mismatch.
4. Source before you generate: run
   `node tools/assets/source/find_assets.mjs --tags "<tags>" --kind <kind>` —
   reuse a shared-library hit, or search the printed free CC0/OFL sources and
   intake one. Only for what cannot be sourced create an art job with
   `tools/assets/job/new_art_job.mjs` (record the decision via `find_assets --record`).
5. Inspect generated outputs before integration; reject weak, unreadable,
   watermarked, fused, or drifting assets.
6. Validate in primary runtime with screenshot evidence plus product gate; hand
   off with `node tools/product_gate/close_slice.mjs`. For `lead-rejection` tasks, strict
   close needs `--resolved-rejection` with exact issue and proof.

## Stop Conditions

- Shape-renderer rectangles, debug buttons, raw text panels, or programmer art
  cannot solve polished/final/generated-art requests.
- A technical build/audit pass is not a beauty pass. If the screenshot still
  reads as tooling, the visual task remains open.
- Product gate fail or lead rejection blocks feature/content expansion unless
  the lead accepts that debt.

---
name: game-visual-art-direction
description: Use when defining, generating, reviewing, integrating, or improving game visual direction, art assets, UI kits, fake shots, sprites, icon sets, generated visuals, child-friendly visual polish, release-quality presentation, or replacing placeholder/procedural visuals with production-style bitmap assets.
---

# Game Visual Art Direction

Use when game visual quality is part of the task: generated art, sprites, UI
kits, fake shots, polished builds, or release-quality presentation. For reusable
runtime UI kits, pair with `.codex/skills/generated-game-ui-assets/`.

## Load Only What Applies

- `references/visual-workflow-gates.md`: accepted visual target, reference
  deconstruction, screen grammar, mismatch audit, Art-First Gate, Reusable UI
  Gate, generated asset rules, report shape.
- `gamedesign/knowledge/reference_deconstruction.md`: named references or
  lead-rejected reference mismatch.
- `.codex/skills/generated-game-ui-assets/`: reusable runtime UI kit production.
- `.codex/skills/game-asset-pipeline/`: art jobs, manifests, provenance, packs.
- `.codex/skills/game-runtime-automation/`: native screenshot/video/runtime
  proof.

## Default Route

1. Load active task, accepted visual target, screenshot, runtime harness, and
   only relevant project/reference sections.
2. Write the 5-line visual session contract: goal, non-goal, proof, stop
   condition, likely files.
3. Produce Reference Digest before final art when named refs drive the look;
   include current screenshot-vs-target mismatch.
4. For multi-asset work, create an art job with `tools/assets/job/new_art_job.mjs`.
5. Inspect generated outputs before integration; reject weak, unreadable,
   watermarked, fused, or style-drifted assets.
6. Validate in primary runtime with screenshot evidence plus
   `node tools/product_gate/review.mjs` or `node tools/ai.mjs gate`; hand off
   with `node tools/ai.mjs close-slice`.

## Stop Conditions

- Shape-renderer rectangles, debug buttons, raw text panels, or programmer art
  cannot solve polished/final/generated-art requests.
- A technical build/audit pass is not a beauty pass. If the screenshot still
  reads as tooling, the visual task remains open.
- Product gate fail or lead rejection blocks feature/content expansion unless
  the lead accepts that debt.

---
name: game-visual-art-direction
description: Use when defining, generating, reviewing, integrating, or improving game visual direction, art assets, UI kits, fake shots, sprites, icon sets, generated visuals, child-friendly visual polish, release-quality presentation, or replacing placeholder/procedural visuals with production-style bitmap assets.
---

# Game Visual Art Direction

Use this skill when visual quality is part of the game task: polished prototypes,
generated art, UI, sprites, fake shots, child-testable builds, or
release-quality presentation.

For reusable generated runtime UI kits, also use
`.codex/skills/generated-game-ui-assets/`; it owns art bible -> slice9 ->
composition proof -> responsive layout audit -> runtime proof.

## Load Only What Applies

- `references/visual-workflow-gates.md`: visual session contract, Reference
  Intake/Digest, Source Ladder, Art-First Gate, Reusable UI Gate, generated asset
  rules, Visual Review Checklist, and report shape.
- `gamedesign/knowledge/reference_deconstruction.md`: full reference method when
  a named reference drives final art.
- `.codex/skills/generated-game-ui-assets/`: reusable runtime UI kits.
- `.codex/skills/game-asset-pipeline/`: art jobs, manifests, provenance, packs.
- `.codex/skills/game-runtime-automation/`: native screenshot/video/runtime
  proof.

## Minimal Route

1. Read project rules, active task, accepted visual target, screenshot, and
   runtime harness.
2. Write the 5-line visual session contract: goal, non-goal, proof, stop
   condition, likely files.
3. For named references, produce Reference Digest before final art.
4. Before integration, write the screenshot-vs-target mismatch list; after
   changes, capture a new native screenshot and update the named mismatch.
5. For multi-asset/reusable UI work, create an art job with
   `tools/assets/job/new_art_job.mjs`; keep provenance/composition data in files.
6. Inspect generated outputs before integration and reject weak, unreadable,
   watermarked, fused, or style-drifted assets.
7. Validate in the primary runtime with screenshot evidence and
   `node tools/product_gate/review.mjs` or `node tools/ai.mjs gate`; use
   `node tools/ai.mjs close-slice` for handoff when available.

## Stop Conditions

- Shape-renderer rectangles, debug buttons, raw text panels, or programmer art
  cannot be the main visual solution for polished/final/generated-art requests.
- A technical build/audit pass is not a beauty pass. If the screenshot still
  reads as tooling, the visual task remains open.
- Product gate fail or lead rejection blocks feature/content expansion unless
  the lead explicitly accepts that debt.

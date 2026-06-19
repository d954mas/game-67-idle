---
name: game-visual-art-direction
description: Use when defining, generating, reviewing, integrating, or improving game visual direction, art assets, UI kits, fake shots, sprites, icon sets, generated visuals, child-friendly visual polish, release-quality presentation, or replacing placeholder/procedural visuals with production-style bitmap assets.
---

# Game Visual Art Direction

Use this skill when visual quality is part of the game task: polished
prototypes, generated art, UI, sprites, fake shots, child-testable builds, or
release-quality presentation.

For reusable generated runtime UI kits, also use
`.codex/skills/generated-game-ui-assets/`; it owns art bible -> slice9 ->
composition proof -> responsive layout audit -> runtime proof.

## Load Only What Applies

- `references/visual-workflow-gates.md`: visual session contract, reference
  intake, source ladder, art-first gate, reusable UI gate, generated asset rules,
  visual review checklist, and report shape.
- `gamedesign/knowledge/reference_deconstruction.md`: full reference method
  when a named reference drives final art.
- `.codex/skills/generated-game-ui-assets/`: reusable runtime UI kits.
- `.codex/skills/game-asset-pipeline/`: art jobs, source sheets, crop manifests,
  atlases, provenance, and runtime packs.
- `.codex/skills/game-runtime-automation/`: native screenshot/video/runtime
  proof.

## Minimal Route

1. Read local project rules, active task, visual target, current screenshot, and
   runtime harness.
2. Write the 5-line visual session contract from
   `references/visual-workflow-gates.md`: goal, non-goal, proof, stop condition,
   likely files.
3. Identify the accepted visual target. If none exists, create or request one
   before integration.
4. For named references, load the reference workflow and produce the Reference
   Digest before final reference-driven art.
5. Write the screenshot-vs-target mismatch list before code/art integration;
   after meaningful changes, capture a new native screenshot and update the
   named mismatch list.
6. For multi-asset or reusable UI work, create/update an art job with
   `tools/assets/job/new_art_job.mjs` and keep source/provenance/composition
   data in files, not chat.
7. Inspect generated outputs before integration and reject weak, unreadable,
   watermarked, fused, or style-drifted assets.
8. Validate in the primary runtime with screenshot evidence and
   `node tools/product_gate/review.mjs` or `node tools/ai.mjs gate`; for visual
   handoff, use `node tools/ai.mjs close-slice` when available.

## Stop Conditions

- Shape-renderer rectangles, debug buttons, raw text panels, or programmer art
  cannot be the main visual solution for polished/final/generated-art requests.
- A technical build/audit pass is not a beauty pass. If the screenshot still
  reads as tooling, the visual task remains open.
- Product gate fail or lead rejection blocks feature/content expansion unless
  the lead explicitly accepts that debt.

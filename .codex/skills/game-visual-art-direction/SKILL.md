---
name: game-visual-art-direction
description: Use when defining, generating, reviewing, integrating, or improving game visual direction, art assets, UI kits, fake shots, sprites, icon sets, generated visuals, child-friendly visual polish, release-quality presentation, or replacing placeholder/procedural visuals with production-style bitmap assets.
---

# Game Visual Art Direction

Use this skill when visual quality is part of the game task: polished
prototypes, child-testable builds, generated art, UI, sprites, fake shots, or
release-quality presentation.

For reusable generated runtime UI kits, also use
`.codex/skills/generated-game-ui-assets/`; it owns art bible -> slice9 ->
composition proof -> responsive layout audit -> runtime proof.

## Load Only What Applies

- `references/visual-workflow-gates.md`: 5-line visual session contract,
  Reference Intake, Definition of Ready, Source Ladder, Reference Evidence
  Board, Reference Digest, screenshot-vs-target mismatch loop, Art-First Gate,
  Reusable UI Gate, Visual Review Checklist, Report Shape.
- `gamedesign/knowledge/reference_deconstruction.md`: full reference
  deconstruction method when a named reference drives final art.
- `.codex/skills/generated-game-ui-assets/`: generated reusable UI kits.
- `.codex/skills/game-asset-pipeline/`: asset pack/manifests/crop pipeline.
- `.codex/skills/game-runtime-automation/`: native screenshot/video/runtime
  proof.

## Minimal Workflow

1. Read local project rules, active task, visual target, current screenshot, and
   runtime harness.
2. Write the 5-line visual session contract: goal, non-goal, proof, stop
   condition, likely files. Proof must name native screenshot/product gate/art
   audit.
3. Identify the accepted visual target: reference, fake shot, art bible, lineup,
   or screenshot. If none exists, create one target first.
4. For named references, do not start final reference-driven art from memory.
   Load the reference workflow and produce the Reference Digest before
   implementation.
5. Before code or art integration, write the screenshot-vs-target mismatch list.
   After each meaningful change, capture a new native screenshot and update the
   named mismatch list before expanding features/content.
6. For multi-asset work, create/update an art job with
   `tools/assets/job/new_art_job.mjs`. Record accepted target, reusable kind,
   candidate policy, must-not-bake list, crop ids, expected runtime composition,
   and slice9 insets.
7. Produce visual assets before polishing placeholder render code when the user
   asks for beautiful, final, generated, release-quality, or child-testable
   visuals.
8. Inspect generated outputs before integration. Reject unreadable text, wrong
   subject, weak silhouettes, random logos, watermarks, style drift, or
   procedural/programmer-art final replacements.
9. Integrate the smallest asset path that proves the visual direction in the
   primary runtime.
10. Validate with primary-runtime screenshots and compare against the target.
    Use `node tools/product_gate/review.mjs` or `node tools/ai.mjs gate`; for
    visual handoff, use `node tools/ai.mjs close-slice` when available.
11. If the lead rejects visuals as ugly, unclear, toy-like, debug-looking, or
    not product-quality, freeze feature/content expansion and create the next
    screenshot proof before more gameplay content.

## Stop Conditions

- Shape-renderer rectangles, debug buttons, raw text panels, or programmer art
  cannot be the main visual solution for a polished/final/generated-art request.
- A technical build/audit pass is not a beauty pass. If the screenshot still
  reads as tooling, the visual task remains open.
- Product gate fail or lead rejection blocks feature/content expansion unless
  the lead explicitly accepts that debt.
- Do not keep crop coordinates, slice9 margins, reusable UI composition, or
  source provenance only in chat history.

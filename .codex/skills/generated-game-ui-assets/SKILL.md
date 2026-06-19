---
name: generated-game-ui-assets
description: "Use when generating, cutting, validating, integrating, or reviewing reusable game UI asset kits from AI art: UI source sheets, icon sheets, slice9 panels/buttons, art bibles, crop manifests, runtime manifests, chroma/alpha cleanup, contact sheets, composition proofs, source-derivation audits, responsive UI layout audits, desktop/portrait screenshot proof, or fixing cropped/fringed generated UI assets."
---

# Generated Game UI Assets

Use this skill for production generated runtime UI. It routes source art,
manifests, composition proofs, runtime screenshots, and responsive proof without
loading every art/asset workflow up front.

## Load Only What Applies

- `references/ui-workflow-gates.md`: art bible, art job, generation records,
  source families, manifests, gate tiers, runtime integration, and Report Shape.
- `references/ui-asset-rules.md`: slice9/content bounds, atlas reuse, cleanup,
  anchors, source-derived PNGs, and desktop/portrait layout rules.
- `game-visual-art-direction`, `game-asset-pipeline`, or
  `game-runtime-automation`: load only when deeper domain work is required.

## Minimal Workflow

1. Read project rules, active task, visual target, art direction, screenshot,
   and runtime harness.
2. Write the 5-line session contract: source art, manifests, proof, screenshot
   gate, and runtime integration evidence.
3. Use one art job as the work unit: art bible -> source family -> intake ->
   manifests -> contact sheet -> composition proof -> runtime/product gate.
4. Keep source families separate; full mockups are visual targets only.
5. Record provenance with `new_generation_record.mjs`; never integrate from an
   empty crop manifest or runtime manifest.

## Stop Conditions

- Reject baked text, fake letters, fused controls, tight gutters, clipped
  silhouettes, key-color contamination, or weak isolation.
- Product/readability fail, lead rejection, or mobile density fail freezes
  feature/content expansion until the earliest failed stage is fixed.
- Technical crop/audit pass is not a beauty pass; judge the assembled screen.
- Procedural/programmer-art scaffolds are debug debt unless accepted by lead.

## Tier Reminder

Use the reference tier matrix:
DRAFT = intake plus runtime PNG/contact sheet; INTEGRATE = strict validation,
composition proof, derivation audit, product/runtime proof; FINAL-ART = source
coverage, slice9/atlas/runtime audits, `validate_art_job.mjs --final-art`,
native screenshots, and responsive proof when applicable.

---
name: generated-game-ui-assets
description: "Use when generating, cutting, validating, integrating, or reviewing reusable game UI asset kits from AI art: UI source sheets, icon sheets, slice9 panels/buttons, art bibles, crop manifests, runtime manifests, chroma/alpha cleanup, contact sheets, composition proofs, source-derivation audits, responsive UI layout audits, desktop/portrait screenshot proof, or fixing cropped/fringed generated UI assets."
---

# Generated Game UI Assets

Use this skill for production generated runtime UI. It routes visual target,
source sheets, crops, runtime manifests, composition proofs, and responsive
proof without loading every art/asset workflow up front.

## Load Only What Applies

- `references/ui-workflow-gates.md`: art bible, art job, generation records,
  source families, source intake, crop/runtime manifests, gate tiers, runtime
  integration, failure response, and Report Shape.
- `references/ui-asset-rules.md`: slice9/content bounds, atlas reuse,
  trim/bleed/extrusion, icons/sprites, gutters, key-color cleanup, anchors, and
  responsive desktop/portrait layout rules.
- `game-visual-art-direction`, `game-asset-pipeline`, or
  `game-runtime-automation`: load only when deeper domain work is required.

## Minimal Workflow

1. Read project rules, active task, visual target, art direction, screenshot,
   and runtime harness.
2. Write the 5-line session contract: source art, manifests, composition proof,
   screenshot/product gate, runtime integration evidence.
3. Use one art job as the work unit: art bible -> source family -> intake ->
   crop/runtime manifests -> contact sheet -> composition proof -> runtime
   screenshot/product gate.
4. Keep source families separate: `blank UI kit sheet`, `isolated icon sheet`,
   and full mockups as visual targets only.
5. Record provenance with `new_generation_record.mjs` using non-empty workflow
   JSON or `--no-seed-reason`.
6. Never integrate from an empty crop manifest or runtime manifest.
7. Pick draft, integrate, or final-art tier from the reference and report
   source art, manifests, proof, validations, and the next visual gap.

## Stop Conditions

- Reject source sheets with baked text, fake letters, fused controls,
  tight gutters, clipped silhouettes, key-color contamination, or weak isolation.
- Product/readability fail, lead visual rejection, or mobile density failure
  freezes feature/content expansion until the earliest failed stage is fixed.
- Technical crop/audit pass is not a beauty pass; assembled screen quality is
  still judged against the art bible/fake shot.
- Procedural/programmer-art scaffolds are debug debt unless the lead explicitly
  accepts them.

## Tier Reminder

- DRAFT: intake plus runtime PNG/contact sheet.
- INTEGRATE: strict art job validation, composition proof,
  `audit_generated_source_derivation.py`, product gate/runtime proof.
- FINAL-ART: source family coverage, slice9/atlas/runtime usage audits,
  source-derived PNGs, `validate_art_job.mjs --final-art`, native/runtime
  screenshots, and responsive layout proof when applicable.

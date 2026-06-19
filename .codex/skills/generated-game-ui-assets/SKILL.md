---
name: generated-game-ui-assets
description: "Use when generating, cutting, validating, integrating, or reviewing reusable game UI asset kits from AI art: UI source sheets, icon sheets, slice9 panels/buttons, art bibles, crop manifests, runtime manifests, chroma/alpha cleanup, contact sheets, composition proofs, source-derivation audits, responsive UI layout audits, desktop/portrait screenshot proof, or fixing cropped/fringed generated UI assets."
---

# Generated Game UI Assets

Use for production generated runtime UI. Keep this entrypoint as a router; load
deep art, asset, or runtime workflows only when needed.

## Load Only What Applies

- `references/ui-workflow-gates.md`: art bible, art job, source families,
  provenance, manifests, contact sheet, proof, runtime integration, gate tiers,
  Report Shape.
- `references/ui-asset-rules.md`: slice9/content bounds, atlas reuse, cleanup,
  source-derived PNGs, desktop/portrait layout, and responsive rules.
- `game-visual-art-direction`, `game-asset-pipeline`, or
  `game-runtime-automation`: load only when deeper domain work is required.

## Minimal Workflow

1. Read project rules, active task, visual target, screenshot, runtime harness.
2. Write the 5-line session contract: source art, manifests, proof, screenshot
   gate, and runtime integration evidence.
3. Use one art job as the unit: art bible -> source family -> intake ->
   manifests -> contact sheet -> composition proof -> runtime/product gate.
4. Keep source families separate; full mockups only as visual targets.
5. Record provenance; never integrate from an empty crop/runtime manifest.

## Stop Conditions

- Reject baked text, fake letters, fused controls, tight gutters, clipped
  silhouettes, key-color contamination, or weak isolation.
- Product/readability, lead, or mobile-density fail freezes feature/content
  expansion until the earliest failed stage is fixed.
- Technical crop/audit pass is not a beauty pass; judge the assembled screen.
- Procedural/programmer-art scaffolds are debug debt unless lead-accepted.

## Tier Reminder

Use the reference tier matrix: DRAFT, INTEGRATE, or FINAL-ART. Final claims need
`validate_art_job.mjs --final-art`, native screenshots, and responsive proof.

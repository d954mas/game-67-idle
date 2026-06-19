---
name: generated-game-ui-assets
description: "Use when generating, cutting, validating, integrating, or reviewing reusable game UI asset kits from AI art: UI source sheets, icon sheets, slice9 panels/buttons, art bibles, crop manifests, runtime manifests, chroma/alpha cleanup, contact sheets, composition proofs, source-derivation audits, responsive UI layout audits, desktop/portrait screenshot proof, or fixing cropped/fringed generated UI assets."
---

# Generated Game UI Assets

Use this skill for production generated runtime UI. It coordinates
`game-visual-art-direction`, `game-asset-pipeline`, and
`game-runtime-automation`; load those skills only when their deeper domain work
is actually needed.

## Load Only What Applies

- `references/ui-workflow-gates.md`: art bible, art job, generation records,
  source families, prompt packets, source intake, crop plans, runtime assets,
  gate tiers, runtime integration, failure response, Report Shape.
- `references/ui-asset-rules.md` # Slice9 Rules: slice9, content safe areas,
  stretch policy, `usage_policy`, overlay assets.
- `references/ui-asset-rules.md` # Atlas And Reuse Rules: atlas metadata,
  review atlases, trim/bleed/extrusion, aliasing, scale variants.
- `references/ui-asset-rules.md` # Icon And Sprite Rules: gutters, key-color
  isolation, `key_matte`/`dual_plate`, pivots and anchors.
- `references/ui-asset-rules.md` # Responsive UI Rules: desktop/portrait split,
  one full-width primary action, touch/action bounds.

## Minimal Workflow

1. Read project rules, active task, visual target, art direction, screenshot,
   and runtime harness.
2. Write the 5-line session contract naming manifests, composition proof,
   screenshot/product gate, and runtime integration evidence.
3. Use one art job as the work unit: art bible -> source family -> intake ->
   crop/runtime manifests -> contact sheet -> composition proof -> runtime
   screenshot/product gate.
4. Generate separate source families such as `blank UI kit sheet` and
   `isolated icon sheet`; use full mockups only as visual targets.
5. Record accepted source provenance with `new_generation_record.mjs`, using
   non-empty workflow JSON or `--no-seed-reason`.
6. Run source intake/crop/build through the referenced workflow. Never integrate
   from an empty crop manifest or runtime manifest.
7. Choose draft, integrate, or final-art tier from the reference, then report
   source art, manifests, proof, gates, validations, and next visual gap.

## Stop Conditions

- Baked text, fake letters, fused controls, tight gutters, clipped silhouettes,
  unsafe key-color contamination, or weak icon/component isolation: reject the
  source sheet before runtime integration.
- Product/readability fail, lead visual rejection, or mobile density failure:
  freeze feature/content expansion and fix the earliest failed stage.
- Technical crop/audit pass is not a beauty pass. A clean crop of bad art still
  fails the assembled screen against the art bible/fake shot.
- Procedural or programmer-art scaffolds are debug exceptions only; they cannot
  close final generated UI work unless the lead explicitly accepts that debt.

## Tier Reminder

- DRAFT: intake plus runtime PNG/contact sheet.
- INTEGRATE: strict art job validation, composition proof,
  `audit_generated_source_derivation.py`, product gate/runtime proof.
- FINAL-ART: source family coverage, slice9/atlas/runtime usage audits,
  source-derived PNGs, `validate_art_job.mjs --final-art`, native/runtime
  screenshots, and responsive layout proof when applicable.

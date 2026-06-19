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

1. Read project rules, active task, visual target, art direction, current
   screenshot, and current runtime harness.
2. Write the 5-line session contract: goal, non-goal, proof, stop condition,
   likely files. Proof must include source/runtime manifests, composition
   proof, native screenshot/product gate, and runtime integration evidence.
3. Create or update the art bible and one art job packet:
   `node tools/assets/job/new_art_job.mjs`.
4. Generate separate source families, not a composed UI screenshot. Required
   families commonly include `blank UI kit sheet`, `isolated icon sheet`,
   decor/state overlays, bars, map/world layers, and sprites/FX. Use full
   mockups only as visual targets.
5. Record accepted source provenance with
   `node tools/assets/job/new_generation_record.mjs`. Use real workflow
   provenance or non-empty workflow JSON; use `--no-seed-reason` when a stable
   seed is unavailable.
6. Run source intake before slicing:
   `normalize_source_sheet_chroma.py` when needed, then
   `audit_source_sheet_intake.py`.
7. Create crop/runtime manifests. Never integrate from an empty crop manifest or
   empty runtime manifest.
8. Build runtime PNGs/contact sheet from accepted source art. Builders must not
   redraw panels with procedural shapes and present them as generated assets.
   Reuse `tools/assets/chroma_key_alpha.py` for chroma/alpha cleanup.
9. Run the tier that matches the iteration:
   draft for source/crop checks, integrate for runtime composition proof and
   `audit_generated_source_derivation.py`, final for the full `--final-art`
   reusable kit gate.
10. Validate the assembled screen with screenshot/product gate evidence:
    `node tools/product_gate/review.mjs` or `node tools/ai.mjs gate`. Use
    `responsive_layout_audit.mjs` when UI bounds exist.
11. Update the art job/task log with source art, crop/runtime manifests,
    contact sheet, composition proof, source-derivation audit, responsive layout
    audit, screenshots, product gates, validations, and the next visual gap.

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

- DRAFT: intake + runtime PNG/contact sheet, optional draft
  `validate_art_job.mjs`.
- INTEGRATE: strict art job validation, composition proof,
  `audit_generated_source_derivation.py`, product gate/runtime proof.
- FINAL-ART: source family coverage, slice9/atlas/runtime usage audits,
  source-derived PNG proof, `validate_art_job.mjs --final-art`, native/runtime
  screenshots, and responsive layout proof when applicable.

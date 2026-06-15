---
id: T0007
title: Research generated art and UI/UX asset pipeline skill
status: done
epic: ""
priority: P1
tags: [visual, art, ui, ux, pipeline, skill]
created: 2026-06-14
updated: 2026-06-15
---

## What

Research and codify a reusable workflow for AI-generated game art and UI/UX
asset production. The current weakness is not image generation quality; it is
turning attractive generated images into usable runtime assets: correctly
separated UI parts, slice9/stretchable panels, button states, icons, sprites,
crop manifests, scale rules, and native screenshot proof.

The output should become a project skill or skill upgrade that agents can use
before generating final art for a game screen.

## Done when

- [x] Research notes summarize how other game/UI pipelines prepare runtime art
  from generated or concept imagery, with source links or local source notes.
- [x] The workflow separates visual direction, source generation, asset
  cutting, slice9/stretch rules, state variants, packing, and runtime proof.
- [x] The workflow defines rejection rules for baked text, fused icons,
  uncuttable panels, non-repeatable backgrounds, weak silhouettes, and wrong
  scale.
- [x] A reusable skill or skill update exists for generated game art/UI asset
  production.
- [x] `tools/assets/new_art_job.mjs` and/or a companion validator supports the
  workflow well enough to catch the previous failure mode.
- [x] A small example art job demonstrates the expected source sheet,
  crop/slice manifest, runtime manifest, and screenshot/product gate evidence.

## Open questions

- Should this become a new dedicated skill, or a major upgrade to
  `game-visual-art-direction` plus `game-asset-pipeline`?
- Which references should drive the first research pass: casual web RPGs,
  mobile RPG UI kits, generated UI asset workflows, or specific engine/UI
  slicing docs?

## Log

- 2026-06-14: Created after reflection on Rune Marches visual failure. The next
  pipeline gap is generated-art production: the agent can create attractive
  images, but failed to reliably cut, stretch, separate, and integrate them as
  runtime game assets.
- 2026-06-14: Added external research source notes at
  `gamedesign/sources/generated_game_ui_asset_pipeline_research_2026-06-14.md`
  covering Unity/Android/Phaser/PixiJS nine-slice docs, Aseprite/TexturePacker
  atlas metadata, ComfyUI/A1111 workflows, public AI game art skills, and
  recent game UI reconstruction papers. Updated `game-visual-art-direction`,
  `game-asset-pipeline`, `AI_PIPELINE.md`, art job scaffold, validator, and
  portable export config. Remaining evidence gap: create one small example job
  with accepted source sheet, crop/slice/runtime manifests, previews, and
  native screenshot/product gate proof.
- 2026-06-14: Created a concrete Rune Marches generated UI kit example:
  `gamedesign/projects/rune-marches/art/ui_design_bible_v2.md`,
  accepted source sheet
  `gamedesign/projects/rune-marches/art/source_sheets/rune-marches-ui-kit-source-v2.png`,
  deterministic slicer `tools/assets/build_rune_marches_ui_kit_v2.py`,
  runtime outputs in `assets/runtime/rune-marches-ui-map-rescue-v2/`, crop and
  runtime manifests, plus contact/slice9 previews in
  `gamedesign/projects/rune-marches/art/previews/`. Draft and strict art-job
  validation pass. Remaining gap before checking final Done item: integrate or
  preview this kit in the native runtime and attach screenshot/product gate
  evidence.
- 2026-06-14: product gate PASS (desktop-ui-kit-v2); review: gamedesign/projects/rune-marches/reviews/product_read_gate_ui_kit_v2_desktop.md; screenshot: tmp/rune_marches/native_ui_kit_v2_first_screen_desktop.png; next: continue to the next narrow slice
- 2026-06-14: product gate FAIL (portrait-ui-kit-v2); review: gamedesign/projects/rune-marches/reviews/product_read_gate_ui_kit_v2_portrait.md; screenshot: tmp/rune_marches/native_ui_kit_v2_first_screen_portrait.png; next: Create a dedicated mobile portrait layout with stacked controls, larger journal content area, and fewer simultaneous status values.
- 2026-06-14: Shoebox-style crop/fringe fix: icon extraction now uses border-connected chroma cleanup, center component isolation, alpha trim, padding, and edge-fringe cleanup; strict validator requires icon trim/component policy; latest desktop gate passes, portrait gate fails for layout density.
- 2026-06-14: Added generated UI pixel audit tool and tests; audit opens runtime PNGs, fails clipped icon alpha bounds and chroma-key edge fringe, caught a builder drift where manifest had trim/component policy but main still used the old crop path; Rune Marches V2 audit now passes for 14 assets.
- 2026-06-14: product gate PASS (desktop-ui-kit-v2); review: gamedesign/projects/rune-marches/reviews/product_read_gate_ui_kit_v2_desktop.md; screenshot: tmp/rune_marches/native_ui_kit_v2_first_screen_desktop.png; next: continue to the next narrow slice
- 2026-06-14: product gate FAIL (portrait-ui-kit-v2); review: gamedesign/projects/rune-marches/reviews/product_read_gate_ui_kit_v2_portrait.md; screenshot: tmp/rune_marches/native_ui_kit_v2_first_screen_portrait.png; next: Create a dedicated mobile portrait layout with stacked controls, larger journal content area, and fewer simultaneous status values.
- 2026-06-14: product gate PASS (desktop-ui-kit-v2); review: gamedesign/projects/rune-marches/reviews/product_read_gate_ui_kit_v2_desktop.md; screenshot: tmp/rune_marches/native_ui_kit_v2_first_screen_desktop.png; next: continue to the next narrow slice
- 2026-06-14: product gate PASS (portrait-ui-kit-v2); review: gamedesign/projects/rune-marches/reviews/product_read_gate_ui_kit_v2_portrait.md; screenshot: tmp/rune_marches/native_ui_kit_v2_first_screen_portrait.png; next: continue to the next narrow slice
- 2026-06-14: Responsive UI composition pass: portrait no longer squeezes desktop layout; compact HUD hides nonessential road value, journal drops dense stat strings, primary action uses a full-width row with secondary buttons below. Desktop and portrait product gates now both pass for UI kit v2.
- 2026-06-14: Added responsive UI layout audit: product_gate tool checks UI-tree action bounds, min touch sizes, non-overlap, and portrait full-width primary row. Rune Marches UI kit v2 desktop and portrait layout audits pass and are referenced from the art bible.
- 2026-06-14: Created dedicated generated-game-ui-assets skill as the orchestrator for AI-generated runtime UI kits. Existing visual/art and asset pipeline skills now point to it for source sheets, slice9 manifests, pixel audits, responsive layout audits, and runtime proof.
- 2026-06-15: Archived during pipeline cleanup: generated-game-ui-assets skill and related validators now exist and passed pipeline validation; future work continues under E003.

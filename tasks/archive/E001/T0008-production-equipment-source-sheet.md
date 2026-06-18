---
id: T0008
title: Production equipment source sheet
status: dropped
epic: E001
priority: P2
tags: [mine-cards, art, equipment, source-sheet, alpha, runtime-assets]
created: 2026-06-17
updated: 2026-06-18
---

## What

Create a production-cut equipment/item source sheet for Mine Cards using the
saved shadow-problem sheet as style/reference input, not as final runtime
source art.

The current saved sheet proves useful silhouettes and material direction, but
its white background, baked shadows, and tight gutters are unsafe for runtime
cropping. The production task is to regenerate or author a clean source family,
slice it through the project asset pipeline, and leave runtime-ready equipment
sprites for later UI/equipment mechanics.

Input evidence:

- Raw reference/probe source:
  `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-sheet-shadow-problem-v001.png`
- Intake fail:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-shadow-problem-v001-intake.md`
- Alpha diagnostic contact sheet:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-sheet-shadow-problem-v001-alpha-contact.png`
- Draft shadowfix source:
  `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-sheet-shadowfix-v001.png`
- Draft shadowfix contact/background previews:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-sheet-shadowfix-v001-contact.png`
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-sheet-shadowfix-v001-bg-preview.png`
- Decision review:
  `gamedesign/projects/mine-cards/reviews/equipment_source_shadow_cutout_review_2026-06-18.md`
- Production art job:
  `gamedesign/projects/mine-cards/art_requests/mine-cards-equipment-source-v001.json`
- Generation prompt packet:
  `gamedesign/projects/mine-cards/art/prompts/mine-cards-equipment-source-v001-prompt.md`
  `gamedesign/projects/mine-cards/art/prompts/mine-cards-equipment-source-v001-prompt.json`
- Runtime crop id order:
  `gamedesign/projects/mine-cards/data/mine-cards-equipment-source-v001-ids.txt`
- Accepted draft production source:
  `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-v001-candidate-b-resheet.png`
- Passing source intake:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-candidate-b-resheet-intake.md`
- Passing semantic/style audit:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-candidate-b-resheet-semantic-audit.md`
- Generation record:
  `gamedesign/projects/mine-cards/art/generation_records/mine-cards-equipment-source-v001-candidate-b-resheet.json`

Scope:

- production source sheet for equipment/item sprites;
- flat safe key background or true transparency;
- no baked cast shadows in the item source;
- optional separate shadow sprites only if needed for UI composition;
- named crop plan, crop manifest, runtime asset manifest, and contact sheet.

Out of scope:

- adding equipment mechanics;
- integrating equipment into the T0001 Mining first screen while T0001 is still
  awaiting lead acceptance;
- using the alpha probe as final runtime art;
- editing `external/neotolis-engine`.

## Done when

- [x] A new accepted equipment source sheet exists with flat safe chroma or true
      transparent background, no baked cast shadows, at least `48px` gutters,
      and at least `64px` outer margin.
- [x] Source-sheet intake passes or records an explicit dual-plate/split-alpha
      exception with passing extraction evidence.
- [x] A semantic/style review confirms the items match Mine Cards and are not
      fused, clipped, mislabeled, watermarked, or unreadable at gameplay size.
- [x] A named crop plan and crop/runtime manifests define each equipment item,
      semantic role, trim padding, and component isolation policy.
- [x] Runtime PNGs/contact sheet are generated and visually reviewed without
      clipped silhouettes, neighboring fragments, visible key-color fringe, or
      hidden RGB under transparent alpha.
- [x] Pixel audit passes before any native screen integration.
- [x] `visual/art_inventory.md` and relevant art job/generation records point
      at the accepted source and runtime outputs.

## Decisions

- First production source sheet includes a mining-first row, a progression
  trinket row, and one broader later-RPG reference row. This keeps the first
  usable sheet relevant to Mining while preserving the old equipment mood.
- UI shadows are not baked into item art. Generate a separate shadow/source
  layer later only when the equipment/inventory screen needs it.

## Log

- 2026-06-18: Created from shadow/cutout review. The saved white-background
  equipment sheet is useful reference material but failed source intake due
  white highlight/key-color conflicts, tight gutters, and baked shadows. Keep it
  out of runtime packs until a production source family exists.
- 2026-06-18: Added a deterministic shadowfix probe and dark/color background
  preview. It preserves 12 components and is usable for fake shots/concept
  planning, but it remains draft-only and does not close the production-source
  requirement.
- 2026-06-18: Created production art job and prompt packet:
  `mine-cards-equipment-source-v001`. It uses the failed intake evidence to
  require flat `#0000ff` or true transparency, 64px outer margin, 48px gutters,
  no baked cast shadows, and an exact 12-item 3x4 row-major item order. Draft
  art-job validation passes; no candidate source has been generated or accepted
  yet.
- 2026-06-18: Updated `tools/assets/plan_source_sheet_prompt.mjs` so prompt
  packets preserve custom `generation_contract.source_sheet_layout.rows` and
  slot ids from the art job. Evidence:
  `node --test tools/assets/plan_source_sheet_prompt.test.mjs` passes and the
  regenerated equipment prompt contains the three requested rows and all 12
  crop ids.
- 2026-06-18: Generated candidate A/B via built-in imagegen. Raw A failed
  intake on non-flat blue background, unsafe gutters, and key conflicts. Raw B
  used the safer green key but still failed due non-flat background and spacing.
  Deterministic rescue of candidate B produced
  `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-v001-candidate-b-resheet.png`;
  source-sheet intake passes (`12 component(s), closest_gap=142`) and semantic
  style audit passes. Generation record:
  `gamedesign/projects/mine-cards/art/generation_records/mine-cards-equipment-source-v001-candidate-b-resheet.json`.
  This accepts a draft source for slicing, not runtime PNGs or final art.
- 2026-06-18: Built named crop plan, 12 runtime equipment PNGs, contact sheet,
  crop/runtime manifests, and pixel audit. Evidence:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-contact.png`,
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-generated_ui_asset_audit.md`,
  and `node tools/assets/validate_art_job.mjs --job
  gamedesign/projects/mine-cards/art_requests/mine-cards-equipment-source-v001.json
  --strict` passed. Moved to review for lead visual acceptance before marking
  done.
- 2026-06-18: Added acceptance audit:
  `gamedesign/projects/mine-cards/reviews/t0008_equipment_art_acceptance_audit_2026-06-18.md`.
  The runtime sprite set is ready for lead visual acceptance as source/runtime
  equipment art, but `validate_art_job --final-art` is not claimed: the current
  job lacks composition/atlas/family-coverage outputs, which belong to a later
  equipment UI integration task.
- 2026-06-18: Added sprite background proof for the reported shadow concern:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-background-proof.png`.
  It composites the 12 runtime sprites over transparent-grid, dark mine panel,
  warm mine panel, and light inventory-slot backgrounds; `review_count=0`.
  Conclusion: no cutout-shadow blocker is detected, but lead may still reject
  the darker painted material style as an art-direction issue.
- 2026-06-18: Linked T0008 into the unified lead review board:
  `gamedesign/projects/mine-cards/reviews/lead_review_board_2026-06-18.md`.
  T0008 remains in review until the lead accepts the item art or names one
  visual rejection axis.
- 2026-06-18: Closed with Mine Cards test run. Equipment art evidence remains historical/reusable reference, but this is not active game work.

---
id: T0011
title: Equipment UI composition and atlas integration
status: idea
epic: E001
priority: P2
tags: [mine-cards, equipment, ui, atlas, composition, gated]
created: 2026-06-17
updated: 2026-06-18
---

## What

Prepare the post-T0008 equipment/inventory UI composition task that turns the
accepted 12 equipment runtime sprites into an actual reusable UI surface.

This is gated behind:

- T0001 lead acceptance, because the first Mining screen must remain frozen
  until accepted or rejected;
- T0008 lead visual acceptance or a concrete rejection axis for the current
  equipment item art.

Current inputs:

- T0008 source/runtime sprite audit:
  `gamedesign/projects/mine-cards/reviews/t0008_equipment_art_acceptance_audit_2026-06-18.md`
- Equipment source art job:
  `gamedesign/projects/mine-cards/art_requests/mine-cards-equipment-source-v001.json`
- Runtime sprites:
  `assets/runtime/mine-cards-equipment-source-v001/`
- Runtime sprite contact sheet:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-contact.png`
- Composition packet:
  `gamedesign/projects/mine-cards/visual/equipment_ui_composition_packet_v001.md`
- Machine-readable composition contract:
  `gamedesign/projects/mine-cards/data/equipment_ui_composition_contract_v001.json`

Scope:

- generated/artist source family for equipment slots, inventory/equipment
  frames, selected/locked/empty states, and item hover/compare surfaces;
- atlas/composition proof that uses the accepted item sprites without baking
  labels, stats, or item art into UI frames;
- future native proof of a small equipment/inventory panel or tab;
- final-art validation outputs that T0008 intentionally does not claim:
  source derivation, composition proof, atlas metadata, atlas pack/audit, and
  source-family coverage.

Out of scope:

- adding equipment stats or mechanics;
- changing the current T0001 Mining first screen before acceptance;
- replacing the item source sheet unless T0008 is rejected;
- editing `external/neotolis-engine`;
- treating this task as started while it remains `idea`.

## Done when

- [ ] T0001 is accepted or the lead explicitly prioritizes equipment UI before
      T0001 acceptance.
- [ ] T0008 item art is accepted, or this task records the exact item-art
      rejection axis and source rescue path.
- [x] First surface, item scope, item-shadow policy, and required gate outputs
      are defined in a machine-readable composition contract.
- [ ] Equipment UI source families are accepted: slot frames, panel/frame
      pieces, selected/locked/empty states, and optional item shadow layer.
- [ ] Atlas metadata and atlas pack are generated for the equipment UI
      composition.
- [ ] Composition proof shows at least one equipment/inventory panel using the
      12 item sprites with runtime text/state overlays, not baked labels.
- [ ] Native screenshot/product gate proves the equipment/inventory surface is
      readable and does not regress T0001's first-screen baseline.
- [ ] `validate_art_job --final-art` or an equivalent scoped final-art gate
      passes for the equipment UI composition outputs.

## Open questions / lead confirmations

- Default first surface is `compact_item_compare_panel`; Equipment tab comes
  later.
- Item shadows are omitted in the first pass and generated as a separate layer
  only if composition proof shows they are needed.
- First proof uses worn pickaxe, copper pickaxe, and mining helmet; atlas/review
  scope still includes all 12 accepted T0008 sprites.

## Log

- 2026-06-18: Created as a gated idea after T0008 proved source/runtime item
  sprites but did not prove final equipment UI composition, atlas pack, or
  source-family coverage. Added `equipment_ui_composition_packet_v001.md`.
- 2026-06-18: Added machine-readable composition contract and validator:
  `gamedesign/projects/mine-cards/data/equipment_ui_composition_contract_v001.json`
  and `tools/assets/validate_equipment_ui_composition_contract.py`. The default
  first proof is a compact item compare panel, not a full Equipment tab. T0011
  remains gated `idea` until T0001 and T0008 are accepted or lead explicitly
  changes priority.

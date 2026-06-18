# Mine Cards Lead Review Board

Date: 2026-06-18
Status: pending lead decision

This board is the single review surface for moving from preparation into the
next implementation slice. It does not accept or reject tasks by itself.

## Current Gate

T0001 is the blocking gate.

- `T0001 Mine Cards Mining v0.01 first slice`: ready for lead review.
- `T0008 Production equipment source sheet`: ready for lead visual review.
- `T0010`, `T0011`, and `T0012`: gated ideas; do not promote until T0001 is
  accepted or the lead explicitly changes priority.

## T0001 Mining First Slice

Recommendation: accept as the v0.01 Mining baseline if temporary KayKit/Ozz
character art and broad end-of-experiment snapshot debt are acceptable.

Evidence to open:

- Review packet:
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_packet_2026-06-18.md`
- Review sheet:
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_sheet_2026-06-18.png`
- Acceptance audit:
  `gamedesign/projects/mine-cards/reviews/t0001_acceptance_audit_2026-06-18.md`
- Responsive closeout gate:
  `gamedesign/projects/mine-cards/reviews/product_gate_t0001_closeout_v001_2026-06-18.md`
- Core motion proof:
  `build/captures/mine_cards_core_moment_v004.gif`
- Slice hygiene snapshot:
  `build/captures/mine_cards_t0001_slice_hygiene_snapshot.md`

Decision choices:

- Accept: move T0001 to done and promote exactly one next task.
- Reject: keep feature expansion frozen and name one rejection axis.

Known accepted debt if accepted:

- character art is a placeholder production-path proof, not final Mine Cards
  custom character art;
- slice hygiene is a WARN snapshot because the broad experiment touched more
  files than a normal narrow slice;
- equipment UI, mastery progression, and custom character production remain
  separate next-slice decisions.

## T0008 Equipment Source Sheet

Recommendation: accept as later-use source/runtime item art if the current
painted material darkness is acceptable.

Evidence to open:

- Acceptance audit:
  `gamedesign/projects/mine-cards/reviews/t0008_equipment_art_acceptance_audit_2026-06-18.md`
- Runtime contact sheet:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-contact.png`
- Background proof:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-background-proof.png`
- Shadow/cutout follow-up:
  `gamedesign/projects/mine-cards/reviews/equipment_shadow_cutout_followup_2026-06-18.md`
- Edge proof:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-edge-proof.png`
- Pixel audit:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-generated_ui_asset_audit.md`

Decision choices:

- Accept: move T0008 to done, but do not auto-promote equipment UI.
- Reject: keep T0008 in review or move back to active work with one visual
  rejection axis.

Known scope boundary:

- T0008 proves item source/runtime sprites only.
- It does not prove equipment mechanics, equipment UI composition, atlas pack,
  or final generated-art kit status.

## Next Slice After T0001 Acceptance

Default next task:

1. `T0010 Custom Mine Cards voxel miner source pack`

Reason: the fixed action stage is already central to the screen, and replacing
placeholder KayKit/Ozz character art is the highest-impact next production
upgrade before adding more systems.

Prepared packet:

- `gamedesign/projects/mine-cards/visual/custom_voxel_miner_source_packet_v001.md`
- `gamedesign/projects/mine-cards/data/custom_voxel_miner_source_pack_v001.json`

Default production lane: hand-authored Blockbench/Blender source, with generated
raster concepts as reference only.

Other valid choices:

- `T0011 Equipment UI composition and atlas integration`, if T0008 is accepted
  and equipment/inventory identity is now the priority.

Prepared equipment UI packet:

- `gamedesign/projects/mine-cards/visual/equipment_ui_composition_packet_v001.md`
- `gamedesign/projects/mine-cards/data/equipment_ui_composition_contract_v001.json`

Default equipment UI proof: compact item compare panel using worn pickaxe,
copper pickaxe, and mining helmet, with all 12 accepted item sprites kept in the
atlas/review scope.
- `T0012 Mining v0.02 mastery tier-up gameplay slice`, if the priority is
  adding one small replay/progression reason before more art.

Prepared gameplay packet:

- `gamedesign/projects/mine-cards/design/mining_v002_mastery_slice_packet.md`
- `gamedesign/projects/mine-cards/data/mining_v002_mastery_contract.json`

Default gameplay proof: `surface_stone` reaches Mastery I at 10 mastery XP and
shows `3.00s -> 2.91s` or `+3% faster` from the next mining cycle.

## Exact Task Actions

Use `gamedesign/projects/mine-cards/reviews/lead_decision_record_2026-06-18.md`
as the operational command checklist after the lead decision is given.

# T0008 Equipment Art Acceptance Audit

Date: 2026-06-18
Task: `T0008 Production equipment source sheet`
Status: ready for lead visual acceptance as a source/runtime sprite set; not
integrated into the Mining first screen.

## Visual Session Contract

- Goal: decide whether the 12-item equipment source set is ready to accept as
  future Mine Cards equipment/inventory source art.
- Non-goal: integrate equipment into T0001, add equipment mechanics, or claim a
  finished inventory/equipment composition.
- Proof: source intake, semantic/style audit, contact sheet, pixel audit, edge
  proof, and art-job validation result.
- Stop condition: if the lead rejects the item art direction or the stricter
  composition/final-art gate becomes required, keep T0008 in review and create
  a focused rescue/integration task instead of adding mechanics.
- Likely files: T0008, `art_requests/mine-cards-equipment-source-v001.json`,
  `assets/runtime/mine-cards-equipment-source-v001/`, and this review.

## Evidence Checked

- Source sheet:
  `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-v001-candidate-b-resheet.png`
- Runtime contact sheet:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-contact.png`
- Source intake:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-candidate-b-resheet-intake.md`
- Semantic/style audit:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-candidate-b-resheet-semantic-audit.md`
- Pixel audit:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-generated_ui_asset_audit.md`
- Edge proof:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-edge-proof.md`
- Shadow/cutout follow-up:
  `gamedesign/projects/mine-cards/reviews/equipment_shadow_cutout_followup_2026-06-18.md`

## Criteria Audit

| Criterion | Evidence | Verdict |
|---|---|---|
| New production candidate exists | candidate B resheet | proved |
| 12 components are isolated | source intake reports 12 components and closest gap 142px | proved |
| Style matches Mine Cards equipment direction | semantic/style audit pass and contact sheet | proved |
| Runtime PNGs exist | `assets/runtime/mine-cards-equipment-source-v001/` | proved |
| Cutout edge quality is clean | generated asset audit pass and edge proof 0 marks | proved |
| Shadows block extraction | shadow follow-up says no; remaining darkness is internal shading | not a blocker |
| Integrated equipment/inventory composition exists | not in scope | not proved |
| Final-art full UI pipeline gate passes | `validate_art_job --final-art` currently fails because composition/atlas/family-coverage outputs are not recorded | not proved |

## Recommendation

Accept T0008 only as a production equipment source/runtime sprite set, pending
lead visual approval of item style.

Do not treat T0008 as:

- an accepted inventory/equipment screen;
- an equipment mechanics implementation;
- proof that all future composition/atlas/family coverage gates are complete.

If the lead accepts the item art, T0008 can be closed as source-sheet work. The
next equipment step is now tracked separately by T0011:
`tasks/active/T0011-equipment-ui-composition-and-atlas-integration.md`.

If the lead rejects it, reject on one concrete axis:

- too dark/shadow-heavy;
- too fantasy-combat, not mining enough;
- too ornate/noisy at gameplay size;
- wrong item set for the first equipment/inventory slice.

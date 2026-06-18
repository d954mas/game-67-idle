# Next Slice Decision Packet

Date: 2026-06-18
Project: Mine Cards v0.01

## Current Gate

`T0001` is ready for lead review, not done.

Do not expand mechanics while T0001 is unaccepted. The only valid work before
acceptance is:

- review evidence cleanup;
- fixing a named rejection axis;
- preparing source/art/technical packets that do not alter the playable slice.

Lead-review packet:

`gamedesign/projects/mine-cards/reviews/t0001_lead_review_packet_2026-06-18.md`

Decision record:

`gamedesign/projects/mine-cards/reviews/lead_decision_record_2026-06-18.md`

## Decision Needed

The lead needs to choose one of two paths:

1. Accept T0001 as the first Mining baseline.
2. Reject T0001 with one explicit axis to fix.

If rejected, the next slice is not selected yet. Fix only the named issue and
recapture the relevant native proof.

If accepted, start the next slice from the options below.

## Recommended Order After Acceptance

### 1. Custom Mine Cards voxel miner art

Why first now:

- T0007/T0009 have already completed the reusable skeletal/skinned mesh path.
- The current screen passes as a production-path proof, but the miner is still
  placeholder kit art.
- Custom art can now target a known runtime format, socket contract, and CPU
  budget instead of landing on a throwaway renderer interface.

Needed before runtime:

- source/provenance record;
- rig/scale/origin contract;
- socket names for pickaxe and future armor;
- screenshot review against T0001 visual direction.

Task status:

- prep task exists as a gated idea:
  `tasks/active/T0010-custom-mine-cards-voxel-miner-source-pack.md`.
- source packet exists:
  `gamedesign/projects/mine-cards/visual/custom_voxel_miner_source_packet_v001.md`.
- do not promote/start it until T0001 acceptance or an explicit lead priority
  change.

Completed prerequisite:

`tasks/archive/E001/T0007-skeletal-extension-cpu-skinned-mesh-renderer-pat.md`
and `tasks/archive/E001/T0009-skinned-mesh-proof-performance-trim.md`

### 2. T0008: production equipment source sheet

Why second:

- It supports future inventory/equipment UI without adding mechanics now.
- The saved white-background equipment sheet is useful reference material but
  failed production intake and must not enter runtime packs as final source.
- T0008 already produced a production-safe 12-item runtime sprite set; it still
  awaits lead visual acceptance and later integration scope.

Review task:

`tasks/active/T0008-production-equipment-source-sheet.md`

Follow-up prep:

`tasks/active/T0011-equipment-ui-composition-and-atlas-integration.md`
captures the later equipment UI composition/atlas task. Keep it as a gated idea
until T0001 is accepted and T0008 item art is accepted or rejected by the lead.

### 3. Deeper Mining mechanics

Why later:

- T0001 already proves one Mining loop.
- Expanding balance/systems before the visual/animation baseline is accepted
  risks returning to broad, thin systems without a strong first screen.

Possible later mechanics:

- `tasks/active/T0012-mining-v0-02-mastery-tier-up-gameplay-slice.md`
  for the smallest post-acceptance gameplay increment: one visible Node Mastery
  tier-up using existing Mining parameters.
- later, a second resource consumer;
- later, first equipment stat;
- later, offline/progress continuation;
- later, smithing or shop link.

## Review Queue Cleanup

T0007/T0009 are archived as done. T0001 must remain in review until lead
acceptance. T0008 remains in review until the lead accepts or rejects the
equipment art direction.

## Recommendation

Default next action after acceptance:

1. Move T0001 to done.
2. Start a narrow custom Mine Cards voxel miner art/provenance task, or accept
   T0008 visually if equipment UI is the priority.
3. Keep deeper mechanics frozen until the first-screen baseline remains
   accepted.

If the lead prioritizes equipment/inventory visual identity over the custom
miner, accept or reject T0008 first. Do not start deeper mechanics from this
packet while T0001 is still unaccepted.

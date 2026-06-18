# Lead Decision Record

Date: 2026-06-18
Project: Mine Cards v0.01
Status: pending lead decision

## Purpose

This record is the short decision form for moving from preparation/review into
the next implementation slice. It does not change the game by itself.

## Decision A: T0001 Mining First Slice

Evidence:

- Lead packet:
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_packet_2026-06-18.md`
- Lead review board:
  `gamedesign/projects/mine-cards/reviews/lead_review_board_2026-06-18.md`
- Acceptance audit:
  `gamedesign/projects/mine-cards/reviews/t0001_acceptance_audit_2026-06-18.md`
- Review sheet:
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_sheet_2026-06-18.png`

### Accept T0001

Meaning:

- The current native Mining screen becomes the v0.01 baseline.
- KayKit/Ozz miner is accepted only as temporary placeholder art.
- Broad diff is accepted as an end-of-experiment snapshot, not a normal small
  production diff.

Task actions:

- Move `T0001` to `done`.
- Keep T0008 in review unless the lead also accepts item art.
- Promote exactly one next task from gated `idea`.

Recommended next promoted task:

1. `T0010 Custom Mine Cards voxel miner source pack`

Reason:

- T0007/T0009 already proved the reusable skeletal/skinned mesh path.
- Replacing placeholder kit art is the highest-impact next improvement before
  deeper mechanics.

### Reject T0001

Meaning:

- Feature expansion remains frozen.
- Record exactly one rejection axis.
- Fix only that axis and recapture the matching proof.

Allowed rejection axes:

- screen still reads as tooling/repeated boxes;
- actor/action stage not tied strongly enough to Mining;
- lower board hides next goal or upgrade reason;
- portrait or 720x480 stress is unacceptable;
- custom Mine Cards character art is required before baseline acceptance.

## Decision B: T0008 Equipment Source Art

Evidence:

- Acceptance audit:
  `gamedesign/projects/mine-cards/reviews/t0008_equipment_art_acceptance_audit_2026-06-18.md`
- Contact sheet:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-contact.png`
- Background proof:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-background-proof.png`
- Edge proof:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-edge-proof.png`

### Accept T0008

Meaning:

- The 12 runtime equipment sprites are accepted as source/runtime item art for
  later use.
- This does not accept an equipment/inventory screen or equipment mechanics.

Task actions:

- Move `T0008` to `done`.
- Keep `T0011` as gated until T0001 is accepted.
- After T0001 acceptance, promote `T0011` only if equipment UI is the chosen
  next slice.

### Reject T0008

Meaning:

- Keep T0008 in review or move it back to active work with one visual rejection
  axis.
- Do not integrate rejected sprites.

Allowed rejection axes:

- too dark/shadow-heavy;
- too fantasy-combat, not mining enough;
- too ornate/noisy at gameplay size;
- wrong item set for the first equipment/inventory slice.

## Next Slice Choices After T0001 Acceptance

Choose one:

1. `T0010` custom voxel miner source pack.
   Best if the priority is replacing placeholder character art and making the
   fixed action stage feel owned by Mine Cards.
2. `T0011` equipment UI composition and atlas integration.
   Best if T0008 item art is accepted and the priority is equipment/inventory
   visual identity.
3. `T0012` Mining v0.02 mastery tier-up gameplay slice.
   Best if the priority is adding one small reason to repeat Mining after the
   first baseline is accepted.

Default recommendation:

```text
Accept T0001 -> promote T0010 -> keep T0011/T0012 as ideas.
```

## Operational Checklist

Use this only after the lead gives the decision.

### If T0001 Is Accepted

Run:

```powershell
node tools/taskboard/cli.mjs set T0001 --status done
```

Then promote exactly one next task.

Default promotion:

```powershell
node tools/taskboard/cli.mjs set T0010 --status backlog
```

Then update `tasks/STATUS.md`:

- remove T0001 as the current gate;
- set current gate to `T0010 custom Mine Cards voxel miner source pack`;
- keep T0008 in review unless the lead also accepts it;
- keep T0011/T0012 as ideas unless explicitly chosen.

Validation:

```powershell
node tools/taskboard/cli.mjs validate
node tools/ai.mjs validate --file tasks/STATUS.md --file tasks/archive/E001/T0001-mine-cards-mining-v0-01-first-slice.md --file tasks/active/T0010-custom-mine-cards-voxel-miner-source-pack.md
```

### If T0001 Is Rejected

Do not move T0001 to done.

Patch the T0001 log with:

- exact rejection axis;
- screenshot/proof that failed, if named;
- next proof required.

Then keep or move T0001 back to active work:

```powershell
node tools/taskboard/cli.mjs set T0001 --status doing
```

Update `tasks/STATUS.md`:

- current gate remains T0001;
- next priority is only the rejected axis;
- no T0010/T0011/T0012 promotion.

### If T0008 Is Also Accepted

Run:

```powershell
node tools/taskboard/cli.mjs set T0008 --status done
```

Do not promote T0011 automatically unless the lead chooses equipment UI as the
next slice after T0001 acceptance.

### If The Lead Chooses Gameplay Before Character Art

This is an explicit priority override. After accepting T0001, promote T0012
instead of T0010:

```powershell
node tools/taskboard/cli.mjs set T0012 --status backlog
```

Use:

`gamedesign/projects/mine-cards/design/mining_v002_mastery_slice_packet.md`

as the implementation packet.

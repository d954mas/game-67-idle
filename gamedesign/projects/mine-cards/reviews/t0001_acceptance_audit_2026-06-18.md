# T0001 Acceptance Audit

Date: 2026-06-18
Task: `T0001 Mine Cards Mining v0.01 first slice`
Status: ready for lead decision; not lead-accepted yet.

## Scope

This audit checks whether the current evidence is sufficient for the lead to
accept T0001 as the first Mining baseline, without expanding mechanics or
changing the playable screen.

## Evidence Checked

- Lead review sheet:
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_sheet_2026-06-18.png`
- Lead review packet:
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_packet_2026-06-18.md`
- Closeout gate:
  `gamedesign/projects/mine-cards/reviews/product_gate_t0001_closeout_v001_2026-06-18.md`
- Compact UI gate:
  `gamedesign/projects/mine-cards/reviews/product_gate_compact_ui_v003_2026-06-18.md`
- Core motion gate:
  `gamedesign/projects/mine-cards/reviews/core_moment_mining_v004_2026-06-18.md`
- Snapshot hygiene:
  `build/captures/mine_cards_t0001_slice_hygiene_snapshot.md`

## Criteria Audit

| Criterion | Evidence | Verdict |
|---|---|---|
| Native Mining first screen exists | T0001 log and native captures | proved |
| Progress, rewards, node selection, upgrade are visible | closeout gate and four-shot proof | proved |
| Real asset render path is used | runtime captures, UI pack, generated sprite sources | proved for slice baseline |
| Modular 3D miner is animated | core motion GIF/sheet with Ozz/KayKit actor | proved as production-path placeholder |
| Landscape and portrait are authored separately | review sheet and compact UI gate | proved |
| Live-state matrix covers required states | closeout gate lists all required states as covered | proved |
| New player can read goal/reward/next upgrade | closeout product read gate | proved |
| Slice hygiene is acceptable | snapshot hygiene is WARN, not FAIL; broad diff is explicitly accepted as experiment snapshot | acceptable with warning |

## Visual Judgment

Current screen is acceptable as a first Mining baseline if the lead accepts:

- KayKit/Ozz miner as temporary placeholder art;
- future activity chips as first-slice placeholders;
- broad worktree snapshot as end-of-experiment evidence rather than a normal
  small diff.

It should not be called final visual art. Custom Mine Cards character production
is now tracked separately by T0010, and equipment art is tracked by T0008.

## Recommendation

Accept T0001 as the first Mining baseline if the lead agrees that the current
screen direction is good enough to build on.

Do not start deeper Mining mechanics before that decision. If the lead rejects
T0001, record exactly one rejection axis and fix only that axis.


# Equipment Shadow Cutout Follow-Up

Date: 2026-06-18

## Visual Session Contract

- Goal: save the reviewed equipment sheet and decide whether its shadows block correct runtime cutout.
- Non-goal: integrate equipment into the current playable slice or expand mechanics while T0001 is still in lead review.
- Proof: saved source copy, source-sheet intake recheck, runtime contact sheet, generated asset audit, and edge proof.
- Stop condition: if lead rejects the look of baked shadows on gameplay/UI backgrounds, freeze equipment integration and regenerate or retouch the source sheet before runtime use.
- Likely files: `gamedesign/projects/mine-cards/art/candidates/`, `gamedesign/projects/mine-cards/reviews/`, `gamedesign/projects/mine-cards/data/`, and `assets/runtime/mine-cards-equipment-source-v001/`.

## Saved Image

Reviewed copy:

`gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-user-shadow-review-v001.png`

Current production candidate it was copied from:

`gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-v001-candidate-b-resheet.png`

## Verdict

The current `resheet` source is cuttable.

Evidence:

- RGBA source has transparent borders and preserved alpha.
- Source intake recheck passed: 12 components, closest gap `142px`.
- Existing runtime asset audit passed for all 12 item sprites with no detected fringe, green spill, purple halo, bad transparent RGB, or nonzero transparent RGB.
- Edge proof found `0` bad edge marks.
- Background proof found `0` shadow-weight review flags across transparent-grid, dark mine panel, warm mine panel, and light inventory-slot backgrounds.

## Shadow Interpretation

The remaining visible darkness is mostly part of the rendered item shading, not a failed background key. That means a normal cutout can be correct while the art can still feel too shadow-heavy on some UI backgrounds.

The background proof makes the distinction explicit: current runtime sprites do not show a cutout-shadow blocker on the tested backgrounds, but the lead can still reject the darker painted material style as an art-direction issue.

For runtime use there are two different decisions:

- Correct cutout: accepted for this candidate based on the current audits.
- Art direction: still needs lead approval if the baked dark shading looks dirty on the actual game UI.

## Outputs To Review

- Runtime contact sheet: `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-contact.png`
- Edge proof: `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-edge-proof.png`
- Edge proof report: `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-edge-proof.md`
- Background proof: `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-background-proof.png`
- Background proof report: `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-background-proof.md`
- Runtime assets: `assets/runtime/mine-cards-equipment-source-v001/`

## If We Need Cleaner Shadows

Preferred production path:

- regenerate or retouch the source sheet with true transparent background;
- remove baked cast shadows from the background;
- keep internal material shading on the objects;
- optionally generate a separate `item_shadow` layer per item if UI composition needs contact shadows;
- rerun source intake, crop contact sheet, asset audit, and edge proof before integration.

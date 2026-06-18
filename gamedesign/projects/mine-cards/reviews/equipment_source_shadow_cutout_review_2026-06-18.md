# Equipment Source Shadow Cutout Review

Date: 2026-06-18

## Visual Session Contract

- Goal: preserve the equipment silhouettes/style as Mine Cards reference input and decide whether the saved sheet can become runtime-ready source art.
- Non-goal: integrate equipment into the current T0001 first screen or expand mechanics while T0001 is still in lead review.
- Proof: source-sheet intake report, alpha contact sheet, and this review note.
- Stop condition: if source intake fails on key-color/gutter/shadow issues, use the sheet only as reference/fake-shot material and regenerate a production source family before runtime packing.
- Likely files: `art/candidates/`, `reviews/`, `visual/art_inventory.md`, and a follow-up task.

## Input

Raw candidate:

`gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-sheet-shadow-problem-v001.png`

Observed role:

- equipment/item source sheet with weapons, helmet, crown, cloak, armor, rings, charm, and skull scroll;
- good fantasy-mining/RPG material language for future equipment progression;
- not currently a production runtime source sheet.

## Automated Intake

Report:

`gamedesign/projects/mine-cards/reviews/mine-cards-equipment-shadow-problem-v001-intake.md`

Verdict: fail.

Specific findings:

- 12 components detected.
- Closest component gap is `15px`, below the required `24px`.
- 3 components have unsafe border gaps (`22px` or `23px`).
- All 12 components contain exact `#ffffff` key-color-like pixels inside visible art, mostly metal highlights and pale material details.
- Recommended next step is regeneration with safer key color, suggested `#0000ff`.

Interpretation:

The white background is not safe as a key color because the equipment art itself uses white highlights. A simple white-key cut can punch holes into metal/specular areas or leave uncertain edges.

## Alpha Probe

Diagnostic output:

`gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-sheet-shadow-problem-v001-alpha-probe.png`

Contact sheet:

`gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-sheet-shadow-problem-v001-alpha-contact.png`

Result:

- The sheet is salvageable for fake shots, concept boards, and quick visual exploration.
- The probe can separate all 12 objects well enough for review-scale composition.
- The result is not final runtime matting: low-chroma near-white shadows were stripped heuristically, and white highlights remain a known risk.

## Shadowfix Probe

Saved cleaned draft:

`gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-sheet-shadowfix-v001.png`

Crops:

`gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-sheet-shadowfix-v001_crops/`

Contact sheet:

`gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-sheet-shadowfix-v001-contact.png`

Dark/color background preview:

`gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-sheet-shadowfix-v001-bg-preview.png`

Result:

- A deterministic alpha-threshold/haze cleanup still preserves 12 item components.
- The saved output is acceptable for concept boards, fake shots, and visual planning.
- It is still not accepted as production runtime art because the source sheet failed intake on key-color conflicts, gutters, and baked shadow/matte uncertainty.

## Decision

Do not accept this raw sheet as final runtime art.

Use it as:

- style reference for equipment silhouettes and material rendering;
- fake-shot/source-board material;
- prompt input for a regenerated `equipment source sheet` family.

Do not use it as:

- final runtime atlas source;
- proof that equipment assets are production-cut;
- a reason to add equipment mechanics before T0001 is accepted.

## Production Requirement

The next equipment source sheet should be generated or authored with:

- flat `#0000ff` or true transparent background;
- no baked cast shadows on the background;
- optional separate shadow layer if shadows are desired in UI composition;
- at least `48px` gutters between all visible pixels and soft edges;
- at least `64px` outer margin;
- one object per slot in row-major order;
- no baked labels, stat text, rarity tags, or UI frames;
- object pivots/anchors planned before runtime use.

Runtime extraction target:

- `item_<name>.png` for each object;
- optional `item_<name>_shadow.png` only if a separate UI shadow layer is needed;
- crop manifest with semantic roles, trim padding, and component isolation policy;
- pixel audit before any native integration.

## Next Action

Create a backlog task for a production equipment source family. It should stay behind the T0001 lead-review gate unless the lead explicitly shifts priority to equipment art.

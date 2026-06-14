# Rune Marches Blank UI Bases Source V1 Review

status: rejected candidate, useful prompt/audit evidence

## Source

- source sheet: `gamedesign/projects/rune-marches/art/source_sheets/rune-marches-blank-ui-bases-source-v1.png`
- generation record: `gamedesign/projects/rune-marches/art/generation_records/rune-marches-blank-ui-bases-source-v1.json`
- workflow record: `gamedesign/projects/rune-marches/art/workflows/rune-marches-blank-ui-bases-source-v1-imagegen.json`

## What Improved

- This is real generated bitmap art, not procedural/programmer art.
- Visual quality is materially better than the temporary two-color slice9 scaffold.
- The sheet separates eight blank UI base components on a chroma-style background.
- It contains no readable text, fake labels, or fused gameplay icons.

## Why It Is Not Accepted

- Strict source-sheet intake fails because the background is not flat enough for `#ff00ff` tolerance 8; the whole sheet becomes one connected component.
- Lenient intake at tolerance 32 finds eight components, but the closest component gap is only 10 px against the 36 px gate.
- The large modal and tall journal panel still have center-edge diamond ornaments. Those must be separate overlay sprites, not part of slice9 stretch zones.
- Built-in image generation does not expose a stable numeric seed; this is recorded as `no_seed_reason` with workflow/source path metadata.

## Evidence

- strict audit: `gamedesign/projects/rune-marches/reviews/rune-marches-blank-ui-bases-source-v1-intake-audit.md`
- lenient audit: `gamedesign/projects/rune-marches/reviews/rune-marches-blank-ui-bases-source-v1-intake-audit-t32.md`

## Next Prompt Fixes

- Ask for a truly flat chroma background with no antialias or compression noise in the background.
- Require at least 96 px gutters between all components.
- For large panels and journal panels, forbid any medallion/diamond/gem on top, bottom, left, or right edge centers.
- Generate corner caps only in the corners, or generate decor overlays on a separate sheet.
- Ask for fewer assets per sheet if the model cannot maintain gutters.

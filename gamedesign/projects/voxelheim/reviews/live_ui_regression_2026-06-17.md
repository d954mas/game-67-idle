---
type: UI Regression Review
title: Voxelheim Live UI Regression - CTA Text and Purple Edge
description: Postmortem and hotfix evidence for the missed live-state UI regression.
tags: [voxelheim, ui, regression, readability, assets]
checked: 2026-06-17
---

# Live UI Regression - CTA Text and Purple Edge

## Verdict

The previous UI acceptance was invalid for the live gameplay state. It proved an
offline/reward popup moment, not the normal post-offline combat screen where the
player sees Gate repair, Frost Blueprints, HUD resources, combat floaters, and
the main CTA at the same time.

## Why It Came Back

The purple edge returned because the durable source asset
`assets/raw/button.png` still contained chroma-key/magenta edge contamination.
The runtime pack is built from `assets/raw/*.png`, so rebuilding the pack can
bring that edge back unless the source PNG itself is cleaned and the button edge
is audited after pack/capture.

This was a pipeline failure, not just a button-art failure: the earlier pass did
not include a pixel/edge audit for the CTA button and did not capture the exact
live state that showed the problem.

## Why The Bad Screen Was Accepted

- The reviewed screenshot was too narrow: it focused on offline/reward feedback
  and did not include the normal live screen with Blueprints + Gate CTA.
- The visual gate was treated as if it covered all UI states, but it covered only
  the captured state.
- The CTA text reused a combat-label outline treatment on a bright green button,
  creating muddy duplicate black text.
- Floaters were drawn after UI text, so transient reward/damage numbers could
  cover instructional UI.
- The Blocks HUD icon used coin/badge art, which made the second resource read
  as a dirty green coin instead of an icy block.

## Hotfix Evidence

- Native build:
  `cmake --build --preset native-debug --target game_seed`
- Focused live proof:
  `py -3.12 tmp/ui_text_overlap_probe.py 9154 build/captures/ui_text_live_overlap_fix.png`
- Screenshot:
  `build/captures/ui_text_live_overlap_fix.png`
- Zoom:
  `build/captures/ui_text_live_overlap_fix_uizoom.png`
- Before/after zoom:
  `build/captures/ui_text_live_overlap_fix_uizoom_cmp.png`
- Purple edge audit:
  `py -3.12 tmp/audit_cta_purple.py build/captures/ui_text_live_overlap_fix.png`

Audit result:

- `assets/raw/button.png`: 0 purple-like pixels.
- Wide CTA screenshot crop: remaining purple-like matches are outside the button
  edge, in overlapping character/background pixels.

## Remaining Risk

`tools/devapi/ui_readability.py` still warns that CTA text strokes are around
3px in the crop. The zoom is readable and the previous muddy duplicate text is
gone, but this should stay open as a polish/gate issue rather than being called
final product quality.

## New Acceptance Rule

Any future UI/text acceptance must include a live combat screenshot after
offline collect with:

- Frost Blueprints visible;
- Gate CTA affordable;
- active combat/floaters;
- HUD Gold + Blocks visible;
- CTA button edge audited against purple/magenta contamination.

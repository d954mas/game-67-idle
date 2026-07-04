# UI Asset Slicing Rules

This note defines production constraints for generated UI art in `rb-dark-rpg`.
It supersedes "pretty source sheet" evaluation for HUD assets: UI art must be
usable as runtime widgets.

## Core Rule

Generated UI pieces are source material for runtime UI, not baked screenshots.
Text, icons, resource fills, locks, quest markers, counters, highlights, and
localization must be rendered by the game on top of reusable frames.

## Slice9 Candidates

Use 9-slice / scale-9 assets for:

- bottom navigation button frames: normal, selected, locked;
- list rows: normal, active, locked;
- bottom sheet panel;
- top location plaque;
- resource chip;
- HP / XP bar frames;
- generic rectangular modal or panel frames.

For these assets, the center and straight edge bands must be visually tile-safe:
no unique highlights, cracks, emblem-like shapes, gradients, or one-off details
that break when stretched.

## Fixed-Size Candidates

Keep these as fixed-size sprites unless a later layout proves otherwise:

- square utility button frame;
- portrait socket;
- quest marker socket;
- large corner cap blocks, if exported separately.

These can have more specific silhouette identity because they are not expected
to stretch.

## Generation Constraints

Future UI source sheets should request separated construction parts, not only
whole frames:

- corner cap block;
- horizontal edge strip;
- vertical edge strip;
- center fill tile;
- button center fill normal / active / locked;
- panel center fill;
- bar frame left cap / center strip / right cap, or one clean 9-slice bar.

Style remains low-noise Roblox dark fantasy: chunky square slabs, dark
leather/wood centers, dull iron or stone caps, muted red active state, sparse
warm amber accents. Avoid ornate filigree, texture noise, unique center art,
and thin decorative trim.

## Acceptance Checklist

- The asset can stretch horizontally without visible distortion.
- The asset can stretch vertically where required without corner deformation.
- Corners are thick and readable at `960x540`.
- Center fill is calm enough for runtime text/icons.
- Active, locked, and normal states share compatible geometry.
- There is enough transparent padding or clean edge for atlas packing.
- The generated scene mock is treated as a review composition, not the source
  for runtime slicing.

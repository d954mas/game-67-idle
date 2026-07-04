# Bottom Nav V6 Art Brief

Date: 2026-07-04

Status: candidate for artlead review, not production-sliced.

## Direction

V6 keeps the useful part of V5: each button has its own identity. It removes the
rainbow feeling by using muted functional accents instead of saturated color
blocks.

## Keep

- Equal outer button size and equal runtime hitboxes.
- Large central icon field.
- Empty lower trough reserved for runtime text.
- No baked labels in the PNG.
- Blocky dark fantasy material language: iron, stone, wood, leather, parchment.
- Low-noise, readable icon silhouettes.

## Icon Set

- Equipment: blocky iron helmet, small brass accent.
- Journal: burgundy leather book.
- Map: parchment map with restrained teal marker.
- Place: garrison gate/signpost with amber lantern.
- More: stacked plaques/tokens, not ellipsis or modern menu dots.

## Runtime Text Treatment

Render labels at runtime over the lower trough:

- Light warm beige text.
- 1 px dark outline.
- Soft shadow.
- No text baked into source art.

## Remaining Review Points

- Check whether the label trough is tall enough for Russian short labels.
- Check whether `More` reads as "more systems" and not inventory/deck.
- If accepted, next step is canvas cleanup/alpha and production slicing.

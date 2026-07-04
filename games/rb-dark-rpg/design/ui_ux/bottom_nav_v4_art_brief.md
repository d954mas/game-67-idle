# Bottom Nav V4 Art Brief

Date: 2026-07-04

Status: art revision required before implementation.

## Problem With V3

V3 has the right broad material direction, but it is not production-ready:

- button borders/caps are too wide;
- the useful central area for icons is too small;
- icons read too similarly at gameplay size;
- the set feels like five variants of one ornament instead of five distinct
  RPG navigation actions.

Do not integrate v3 as final art unless the artlead explicitly releases it.

## Keep From V3

- dark garrison-token mood;
- blocky Roblox-like dark fantasy material language;
- dark wood/leather face;
- dull iron/stone bevels;
- muted amber highlight;
- muted red-brown selected state;
- equal button dimensions.

## Change For V4

Button frame:

- reduce visual border thickness by roughly 35-45%;
- keep corners readable, but stop them from eating the icon field;
- use a flatter center plate with low noise;
- keep 9-slice-friendly edges and corners;
- leave a clear icon well and label strip.
- do not mirror the top bevel/corner construction into the bottom edge; the
  bottom must have its own weight, contact shadow, and downward-facing bevel so
  the button reads as a physical token, not as vertically flipped art.

Icon direction:

- Equipment: square chest/armor silhouette, broad lid or shoulder-plate read.
- Journal: open book or scroll stack, wide horizontal page shape.
- Map: folded map with one strong diagonal fold and one route mark.
- Place: lantern/signpost/town marker, tall silhouette distinct from map.
- More: three rivets/dots or small tool cluster, deliberately minimal.

Icon rules:

- each icon must be readable as a black/amber silhouette at 960x540;
- avoid tiny straps, cracks, rivets, and decorative curls;
- make shape language different before color/detail;
- icons are separate alpha sprites, not baked into the frame.

Runtime rules remain:

- labels are runtime text;
- badges/locks are overlays;
- all five buttons share size and baseline;
- `Карта` is centered by position only, not enlarged;
- nav sits over the lower fade band, not a hard panel.

## V4 Acceptance

- More central area for icon plus label than v3.
- Each icon is identifiable when viewed small and blurred.
- Frame still feels like the accepted garrison-token style.
- Top and bottom edges are directionally lit, not mirrored copies.
- 9-slice can stretch the frame without distorting icon art.
- No baked Russian labels in PNG.

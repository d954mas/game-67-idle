# Recipe, style, animation, and pack cards

Recipe/style/animation cards are Canvas groups carrying their respective
metadata blobs. Create and patch them through ops/CLI. Generated results land
beside the card in its parent scope, never inside the card where they could
silently become future references.

Recipe pack mode is `recipe.pack` on the same recipe card. Preview expands the
axes contract without generation; generate records per-sheet jobs; pack slice
validates each sheet's detected region count against its cells and reports
`OK`, `REJECT`, or `MISSING`. The run group and frozen parameters are
provenance, not UI-only state.

Card generation and prompt expansion may run outside the project lock, but the
final commit refuses a moved history head. One accepted run is one journal
entry.

## Game style-lock layout

The existing style card remains the generation primitive (`prompt` plus one
generation ref and any number of eyes-only examples). A game style lock does
not add a second card type. It uses one top-level plain `style` group containing
a child `passport` style card plus plain `palette`, `references`, and `do-dont`
groups. The passport holds 2-3 owned world/GUI exemplars; moodboard references
stay outside it. Stable group/element refs are recorded in the game-owned
`design/style_lock.json` contract described in
[Style lock](../../style_lock/README.md).

Create and patch the layout through ops/CLI. Never hand-edit Canvas project
files, and never treat palette distance as deterministic style acceptance.

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

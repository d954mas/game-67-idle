# Template Generated UI Kit

The slice9 plates, pills and the one glyph that every template screen renders
with. Nothing in this pack is sourced: `tools/gen_ui_kit.py` draws all of it
from the `--ui-*` tokens in `design/references/ui_kit/styles.css`, at 4x, and
downsamples with LANCZOS.

Change a colour or a radius in `styles.css`, run the generator, and update the
`sha256` rows in `assets.jsonl`. Do not edit the PNGs: the next regeneration
overwrites them, and the tokens stop being the source of truth.

The slice9 borders in `src/build_packs.c` are stated in design units times the
generator's export scale. The two must move together.

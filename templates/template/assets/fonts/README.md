# Fonts

The shipped face is BUILT, not committed. `cmake/GameAssets.cmake` runs
`tools/merge_font_glyphs.py` over the engine's `LilitaOne-RussianChineseKo.ttf`
and writes `GameDisplay.ttf` into the build tree; the pack step subsets it to
the charset the corpus asks for.

The base needs the pass for two reasons. It carries no Turkish or Polish
letters, and it draws `Ń ń Ё ё` in a lighter, narrower hand borrowed from
another typeface — next to the letters around them they read as a second font
(engine issue
[#441](https://github.com/d954mas/neotolis-engine/issues/441)). The tool
composes all of them from the base's own outlines and marks, so no foreign
curve enters the face.

`Ё` is the one that cannot be written around: Russian copy in this studio
spells `е` (`features/localization`), but a portal hands back player names that
carry `ё`.

## Licence

The base is Lilita One (Juan Montoreano, OFL-1.1) merged with Cyrillic and CJK
faces; `OFL.txt` is that licence and ships with the game. The OFL forbids a
Modified Version from carrying the parent's Reserved Font Name, which is why
the output is `Game Display` and never `Lilita`. A game with its own face
passes `--family` / `--postscript` and records provenance beside its own font.

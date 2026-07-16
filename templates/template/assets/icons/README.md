# Template item icons

Six demo-item icons for the template's Lua Items catalog, packed into
the `icons` atlas by `src/build_packs.c` (region names `icons/<name>`, RECT,
`allow_transform=false`). 128×128 px, RGBA, transparent background.

| File          | Item                    | Color   |
|---------------|-------------------------|---------|
| `gold.png`    | Gold (currency)         | amber   |
| `xp.png`      | Experience (currency)   | blue    |
| `energy.png`  | Energy (currency)       | yellow  |
| `potion.png`  | Healing Potion          | red     |
| `sword.png`   | Iron Sword               | steel   |
| `wood.png`    | Wood (material)         | brown   |

## License decision

The template ships CC0/OFL assets only. All six icons below are CC0; no
attribution requirement remains.

## Source-first search (AGENTS invariant)

1. **Shared asset library**
   (`node ai_studio/assets/catalog/search.mjs --query "item icons gold sword potion" --json`)
   → 0 fitting results.
2. **Kenney CC0** — checked `game-icons`, `generic-items`,
   `ui-pack-rpg-expansion`, `roguelike-rpg-pack`, `ui-pack`,
   `ui-pack-adventure`, and `board-game-icons`:
   - **`kenney.nl/assets/board-game-icons`** (CC0, v1.1, 250 assets) — has a
     `PNG/Double (128px)` folder (native 128×128, no upscale needed) with
     single-color white glyph-on-transparent icons for `sword`,
     `resource_wood` (used for `wood.png`), and `flask_full` (used for
     `potion.png`).
   - **`kenney.nl/assets/game-icons`** (CC0, 105 assets) and
     **`kenney.nl/assets/game-icons-expansion`** (CC0, 60 assets) — `star`
     (used for `xp.png`) and `coin`/`power` (used for `gold.png`/
     `energy.png`) respectively, shipped at 100×100 (`PNG/White/2x`),
     upscaled 100→128 (Lanczos), a 1.28× upscale of anti-aliased flat-glyph
     art.
   No dedicated lightning-bolt/energy glyph was found in any Kenney pack
   checked (`bolt`/`flash`/`zap`/`lightning`/`energy`/`thunder`/`electr` —
   no filename matches in any pack); the CC0 `power` (power-button ⏻) glyph
   from `game-icons` stands in for "energy". It is in the same pack family as
   `coin`/`star`, keeping the 3 currency icons (`gold`/`xp`/`energy`)
   provenance-consistent with each other, and the 3 physical-item icons
   (`potion`/`sword`/`wood`) provenance-consistent with each other (all from
   `board-game-icons`). All 6 are the same Kenney flat-glyph visual language
   (uniform-weight white silhouette, rounded caps) — no pixel-art/vector mix.
3. Generation — not needed, CC0 fit found for all 6.

## Preparation

All 6 source glyphs ship as a single flat white shape (RGB `255,255,255`
uniformly, including fully-transparent pixels — confirmed no color bleed
before the `star`/`coin`/`power` upscale) with anti-aliased alpha on a
transparent background. Each glyph's alpha channel was preserved as-is; only
RGB was replaced with the per-icon theme color (`#FFB300` gold, `#29B6F6` xp,
`#FFEB3B` energy, `#E53935` potion, `#B0BEC5` sword, `#8D6E63` wood).
Recoloring a CC0 work has no license implications (CC0 has no attribution or
share-alike terms).

## Provenance

CC0 1.0 (Creative Commons Zero / Public Domain Dedication) —
https://creativecommons.org/publicdomain/zero/1.0/ — **no attribution
required** (credit to Kenney is appreciated per each pack's `License.txt`
but explicitly "not mandatory").

| File         | Pack (Kenney, CC0)      | Source glyph    | Original size       | URL                                            | sha256 |
|--------------|--------------------------|------------------|----------------------|-------------------------------------------------|--------|
| `gold.png`   | Game Icons (Expansion)   | `coin`           | 100×100 (upscaled)  | https://kenney.nl/assets/game-icons-expansion  | `bc6d1e2b3f2ce81495198defc02a9a48d5ba78697b9f4a432ff43241beb5908a` |
| `xp.png`     | Game Icons               | `star`           | 100×100 (upscaled)  | https://kenney.nl/assets/game-icons            | `c907663e17116beedf86cd5a4df814bd6da1ffefbf90e507740b48a7dc43cea4` |
| `energy.png` | Game Icons               | `power`          | 100×100 (upscaled)  | https://kenney.nl/assets/game-icons            | `1bf743020a400c09a564e2b6047a4e55e7199743b015ce4e25675a20896636db` |
| `potion.png` | Board Game Icons         | `flask_full`     | 128×128 (native)    | https://kenney.nl/assets/board-game-icons      | `3d5d664a7b141bb1efa9d9bb12d71bc6d42141e1ef1f5dc760a39c78837a184d` |
| `sword.png`  | Board Game Icons         | `sword`          | 128×128 (native)    | https://kenney.nl/assets/board-game-icons      | `648d6a21a19583a8bf405b000515ba63ccd5751d47594fac9878253dcb6e2cdd` |
| `wood.png`   | Board Game Icons         | `resource_wood`  | 128×128 (native)    | https://kenney.nl/assets/board-game-icons      | `52eec1a49d5376f49c76358c2457734a3c44c5bb0ccdd52bdca7c1254e9e4df5` |

- **Origin:** sourced
- **License:** CC0 1.0 (Creative Commons Zero) — public domain, free for
  commercial/personal use, no attribution required.
- **Author/source:** Kenney (Kenney Vleugels, https://kenney.nl), packs
  "Board Game Icons", "Game Icons", "Game Icons (Expansion)" (per-file pack
  in the table above). Each pack's bundled `License.txt` names CC0
  (`http://creativecommons.org/publicdomain/zero/1.0/`) explicitly.
- **Integrity:** sha256 of each committed PNG in the table above (computed
  on the recolored file actually committed, not the Kenney source glyph).
- **Acquisition:** source-first search found a full CC0 fit for all six icons;
  no generated fallback was needed.

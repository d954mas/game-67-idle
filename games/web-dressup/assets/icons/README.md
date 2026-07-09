# Template item icons (T0316)

Six demo-item icons for the template's `content/items.json` catalog, packed into
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

## 2026-07-08 replacement — CC0-only decision

Lead decision (2026-07-08): the template ships CC0/OFL assets only. The
original set (T0316 slice 1, commit `84a14c55e`) sourced all 6 icons from
game-icons.net under **CC BY 3.0** (Willdabeast/Delapouite/Lorc) because the
source-first search at the time (`docs/build_spec_icons_2026-07-08.md` §2)
only checked 4 Kenney packs and found no CC0 fit. This round re-did the
source-first search against a wider set of Kenney CC0 packs and found a full
CC0 fit for all 6 concepts — the CC BY set is fully replaced below, no
attribution requirement remains.

## Source-first search (AGENTS invariant)

1. **Shared asset library** — re-checked
   (`node ai_studio/assets/backlog/storage/search.mjs --query "item icons gold sword potion" --json`)
   → same result as before, 0 fitting results.
2. **Kenney CC0** — this round checked packs beyond the 4 from the original
   search (`game-icons`, `generic-items`, `ui-pack-rpg-expansion`,
   `roguelike-rpg-pack`, plus `ui-pack`, `ui-pack-adventure` this round) and
   found a fit in two packs not previously checked:
   - **`kenney.nl/assets/board-game-icons`** (CC0, v1.1, 250 assets) — has a
     `PNG/Double (128px)` folder (native 128×128, no upscale needed) with
     single-color white glyph-on-transparent icons for `sword`,
     `resource_wood` (used for `wood.png`), and `flask_full` (used for
     `potion.png`).
   - **`kenney.nl/assets/game-icons`** (CC0, 105 assets) and
     **`kenney.nl/assets/game-icons-expansion`** (CC0, 60 assets) — `star`
     (used for `xp.png`) and `coin`/`power` (used for `gold.png`/
     `energy.png`) respectively, shipped at 100×100 (`PNG/White/2x`),
     upscaled 100→128 (Lanczos) — a 1.28× upscale of anti-aliased flat-glyph
     art, not the 8× pixel-art upscale the original search rejected for
     `roguelike-rpg-pack`.
   No dedicated lightning-bolt/energy glyph was found in any Kenney pack
   checked (`bolt`/`flash`/`zap`/`lightning`/`energy`/`thunder`/`electr` —
   no filename matches in any pack); the CC0 `power` (power-button ⏻) glyph
   from `game-icons` stands in for "energy" instead — same pack family as
   `coin`/`star`, keeping the 3 currency icons (`gold`/`xp`/`energy`)
   provenance-consistent with each other, and the 3 physical-item icons
   (`potion`/`sword`/`wood`) provenance-consistent with each other (all from
   `board-game-icons`). All 6 are the same Kenney flat-glyph visual language
   (uniform-weight white silhouette, rounded caps) — no pixel-art/vector mix.
3. Generation — not needed, CC0 fit found for all 6.

## Recoloring

All 6 source glyphs ship as a single flat white shape (RGB `255,255,255`
uniformly, including fully-transparent pixels — confirmed no color bleed
before the `star`/`coin`/`power` upscale) with anti-aliased alpha on
transparent background — the same icon-font convention as the previous CC BY
set. Each glyph's alpha channel was preserved as-is; only RGB was replaced
with the per-icon theme color (table above), reusing the exact colors from
the previous CC BY set for visual continuity (`#FFB300` gold, `#29B6F6` xp,
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
- **How it got here:** source-first search (library → wider Kenney CC0 pack
  sweep than the original T0316 round) found a full CC0 fit for all 6 icons;
  no CC BY or generated fallback needed this round.

## History

- 2026-07-06 (T0316 slice 1, commit `84a14c55e`): initial 6-icon set sourced
  from game-icons.net, CC BY 3.0 (Willdabeast/Delapouite/Lorc), attribution
  recorded in this file.
- 2026-07-08: CC BY set replaced by lead decision — template is CC0/OFL-only.
  All 6 files re-sourced from Kenney CC0 packs (`board-game-icons`,
  `game-icons`, `game-icons-expansion`); no CC BY assets remain in this
  directory.

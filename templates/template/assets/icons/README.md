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

## Source-first search (AGENTS invariant)

1. **Shared asset library** —
   `node ai_studio/assets/backlog/storage/search.mjs --query "item icons gold sword potion" --json`
   → 0 results. `--query "icon" --json` → 1 unrelated 3D model
   (poly.pizza `pirate-kit`). No 2D icon set for these 6 concepts exists in the
   library.
2. **Kenney CC0** (canonical no-attribution source, precedent `assets/ui/`) —
   checked three candidate packs, none fit:
   - `kenney.nl/assets/game-icons` — UI glyphs (arrows/buttons/checkmarks), no
     item art.
   - `kenney.nl/assets/generic-items` — real-world office/tool/kitchen items,
     no fantasy/currency icons.
   - `kenney.nl/assets/ui-pack-rpg-expansion` — panel/button/bar chrome, no
     standalone item icons.
   - `kenney.nl/assets/roguelike-rpg-pack` — has fantasy item art, but only as
     an unlabeled 16×16 spritesheet (no per-icon filenames/coordinates to
     identify gold/potion/sword/wood cells without a manual visual grid
     lookup) and a pixel-art style that would need blurry upscaling under the
     atlas's fixed LINEAR filter (opts parity with `ui`, `build_packs.c`).
   Deviation from spec `docs/build_spec_icons_2026-07-08.md` §2 (which
   expected gold/potion/sword to land in Kenney CC0): none of the checked
   Kenney packs had a fitting flat/soft 128×128 icon set for ANY of the 6
   concepts, not just xp/energy/wood.
3. **game-icons.net (CC BY 3.0)** — all 6 icons sourced here, single SVG per
   icon, recolored (see below) and rasterized to 128×128 RGBA PNG.

## Recoloring

game-icons.net's "1×1" SVG delivery format ships a black-square background
path + a white glyph path (icon-font convention). The background path was
removed (transparent canvas) and the glyph path's `fill="#fff"` was replaced
with the per-icon theme color above, then rasterized with headless Chrome
(`--headless=new --default-background-color=00000000`, 128×128 viewport) —
antialiased edges, no additional alpha processing (`convert`/`magick`/
`cairosvg` were not available on this machine; Chrome headless rendering was
the available, deterministic, license-compliant path). Recoloring a CC BY
work is a derivative edit, not a relicense — attribution below still applies
to the original glyph author.

## Provenance

CC BY 3.0 — **attribution mandatory** (author + icon URL), license:
https://creativecommons.org/licenses/by/3.0/

| File         | Author       | Icon                | URL                                                            | sha256 |
|--------------|--------------|----------------------|-----------------------------------------------------------------|--------|
| `gold.png`   | Willdabeast  | `gold-bar`           | https://game-icons.net/1x1/willdabeast/gold-bar.html            | `df770ff81cd6788952c957543d42a1246a87dcd08b8fe659b183d5befcc4f890` |
| `xp.png`     | Delapouite   | `round-star`         | https://game-icons.net/1x1/delapouite/round-star.html           | `88e59bcf06f1d28890a9bef8f4ce421d2564e5cd0b28bf58f39e4b32a4f24d85` |
| `energy.png` | Lorc         | `power-lightning`    | https://game-icons.net/1x1/lorc/power-lightning.html            | `88fd5f47cf4082bdebbb0c407e080d99d4d1bfd237ed6a5f5e186136529d8106` |
| `potion.png` | Delapouite   | `health-potion`      | https://game-icons.net/1x1/delapouite/health-potion.html        | `bf58eb35c78fab96a15013efb345eb7eef6d4fac73a68724215ea8660458e9bb` |
| `sword.png`  | Lorc         | `broadsword`         | https://game-icons.net/1x1/lorc/broadsword.html                 | `6e305c83b45aa386fe29fad8475906504217a6d205b2fd4115c2745431ae2f73` |
| `wood.png`   | Delapouite   | `wood-pile`          | https://game-icons.net/1x1/delapouite/wood-pile.html             | `4741b30113b9d8052729b84df7bfde55b65628414750ec07cf52b0effe0ae0ab` |

- **Origin:** sourced
- **License:** CC BY 3.0 (Creative Commons Attribution 3.0) — free for
  commercial/personal use, attribution required (this file + the table above
  is the attribution record).
- **Author/source:** game-icons.net (https://game-icons.net), per-icon
  authors Willdabeast, Delapouite, Lorc (see table). Source SVGs from
  `github.com/game-icons/icons` (the repo backing the site;
  `license.txt` in that repo names Willdabeast/Delapouite/Lorc as CC BY
  contributors — none of the three is in the repo's explicit CC0 list).
- **Integrity:** sha256 of each committed PNG in the table above.
- **How it got here:** source-first search (library → Kenney CC0 → game-icons.net
  CC BY fallback, in that order, per `docs/build_spec_icons_2026-07-08.md` §2)
  found no CC0 fit; game-icons.net was the working CC BY fallback for all 6,
  not just the 1-2 the spec anticipated.

# UI source sheet 04 provenance

- Game: `rb-dark-rpg`
- Asset role: generated UI source sheet for the first-screen fade HUD direction
- Local source: `games/rb-dark-rpg/assets/ui/generated/roblox_blocky_rpg_tiles_01/source_sheet_04.png`
- Generator: OpenAI image generation via local `nt-asset-image-generation` workflow
- Date: 2026-07-04
- License/origin: generated project asset; keep with this provenance before any production reuse
- SHA256: `3ADDCC98C940FE67848D83A9F1BEE5410CB5048BFC9A03D0A16CFAE2788081DD`

## Art Direction

Low-noise Roblox-like dark fantasy UI kit for runtime text/icons/fills. The accepted direction is chunky block geometry, square slab buttons, broad dark leather/wood centers, charcoal stone or dull iron caps, muted rusty red active state, and sparse warm amber accents.

Avoid using this as a baked full HUD screenshot. Text, icons, HP/XP/resource fills, locks, quest markers, and localization should be rendered by the game/runtime on top of the empty generated frames.

For production slicing rules, see `games/rb-dark-rpg/design/ui_ux/ui_asset_slicing_rules.md`. This v4 sheet is a visual direction candidate; final implementation exports should split Slice9-ready corners, edges, and center fills where widgets need to stretch.

## Prompt

```text
Production-ready LOW-NOISE Roblox dark fantasy RPG UI source sheet, real raster image, 1536x1024, flat solid chroma green background #00ff00 up to every object edge for cutout.

NO text, NO letters, NO numbers, NO icons, NO logos, NO symbols, NO watermarks. Empty UI frames only.

Goal: clean modular UI kit for a 2D illustrated browser RPG that feels like dark Roblox fantasy, not ornate gothic and not sci-fi metal HUD.

Style target:
- chunky Roblox-like dark fantasy
- thick square button tiles, not thin rails
- blocky low-poly bevels
- broad dark leather and dark wood centers
- charcoal stone / dull iron corner blocks
- muted rusty red selected state
- small warm amber edge accents only
- game-readable at 960x540

Low-noise rules:
- large flat matte planes
- 1 or 2 bevel bands per object
- 2 or 3 big value groups per object
- no grain, no scratches, no engraving, no rust speckles
- no tiny trim, no filigree, no busy corners
- sparse big square studs or block caps only
- clean alpha-friendly outer edges

Shape language:
- square corners, chunky cuboid corner caps
- rectangular slab silhouettes
- bottom nav buttons must be thick tile-like bases, about 2.2:1 to 2.7:1 aspect ratio, tall enough for icon and text layered later
- panels must be simple broad dark leather/wood surfaces with blocky stone/iron frame
- fantasy mood through rugged frontier materials, not decoration

Separated source sheet parts with clear spacing:
1 top HUD character frame with square portrait socket and two broad empty horizontal bar slots
2 top center location plaque frame, simple blocky dark leather plaque
3 top resource chip frame, compact thick tile
4 bottom navigation button normal state, chunky tall tile
5 bottom navigation button selected state, chunky tall tile with muted rusty red center and small amber edge
6 bottom navigation button locked/disabled state, chunky tall tile in cold gray
7 compact bottom sheet panel, broad dark leather/wood center
8 list row normal, medium-height block row
9 list row active, medium-height block row with muted red center
10 list row locked, medium-height cold gray block row
11 small square utility button frame
12 HP/XP bar frame small, clean bar frame
13 HP/XP bar frame large, clean bar frame
14 quest marker empty frame, blocky square socket with simple top cap

Hard negatives: no ornate gothic filigree, no scrollwork, no thin golden trim, no cathedral spikes, no skulls, no readable runes, no medieval parchment, no Diablo-style realistic inventory UI, no sci-fi control panel, no futuristic metal HUD, no thin slider-like nav buttons, no fine decorative corners, no texture noise, no scratches, no dirt speckles, no surface grain, no complex painterly detail, no glowing magic UI, no purple neon, no rounded mobile casual buttons, no text, no icons, no characters, no scene background.

The result should read at thumbnail size as clean blocky dark fantasy Roblox RPG UI: warm rugged, thick, simple, readable, production-ready for runtime text/icons/fills layered separately.
```

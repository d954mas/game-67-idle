# Combat Actor Sprites 01 Provenance

- Assets:
  - `slices/combat_actor_hero.png`
  - `slices/combat_actor_gate_scavenger.png`
  - `slices/combat_actor_mill_scavenger.png`
- Game: `rb-dark-rpg`
- Role: separated hero/enemy actor sprites for the animated combat clash stage.
- Status: accepted generated runtime assets, normalized to 512x704 transparent PNGs.
- Origin: AI-generated source sheet processed through chroma-key cutout and crop normalization on 2026-07-05.
- License: project-internal generated asset.

## Source-First Check

Local shared asset search:

```text
node ai_studio/assets/backlog/storage/search.mjs --query "dark rpg hero combat enemy sprite character"
assets: 0 match(es)
```

External free-source review:

- Kenney Roguelike/RPG pack, Creative Commons CC0, https://kenney.nl/assets/roguelike-rpg-pack
- Kenney Tiny Dungeon, Creative Commons CC0, https://kenney.nl/assets/tiny-dungeon

Both candidates were rejected for this combat pass because they are 16x16 top-down pixel packs. They are license-compatible, but they do not match the existing large side-view clash composition or the current dark-RPG raster style.

## Generation

- Tool: `.codex/skills/nt-asset-image-generation/scripts/generate_image.py`
- Transport: Codex/OpenAI image generation helper via Windows `curl`
- Model: `gpt-image-2`
- Requested size: `1536x1024`
- Actual raw size: `1774x887`
- Raw source sheet: `tmp/imagegen/combat_actor_sheet_01_chromakey.png`
- Raw source sheet SHA256: `463E849E41C361619E6983F8C762E817A4CC30A9AA74E5B6C10A36EF06D38C66`
- Cutout sheet: `tmp/imagegen/combat_actor_sheet_01_cutout.png`
- Cutout sheet SHA256: `F4C0876F3C42CF95E8093F223AA561029AC333F8CE5A68C693735A1853CDC4DB`
- Canvas project: `canvas://rb-dark-rpg-combat-actor-sprites-01-b24b3f`

Prompt is stored in `prompt_source_sheet.txt`.

## Processing

Background removal:

```text
C:\Python312\python.exe C:\Users\ROG\.codex\skills\.system\imagegen\scripts\remove_chroma_key.py --input tmp\imagegen\combat_actor_sheet_01_chromakey.png --out tmp\imagegen\combat_actor_sheet_01_cutout.png --auto-key border --soft-matte --transparent-threshold 12 --opaque-threshold 220 --despill
```

Helper output:

```text
Key color: #05f807
Transparent pixels: 1216555/1573538
Partially transparent pixels: 11188/1573538
```

The final runtime PNGs were cropped from the cutout sheet, scaled into a shared 512x704 transparent canvas, horizontally centered, and bottom-aligned with a 22px inset so combat transforms share a stable baseline.

## Integrity

- `combat_actor_hero.png`: `07F1BABBC14C698175F6E056AB29B80250A63DBFF8D28BB0A3C0C23BDA71579F`
- `combat_actor_gate_scavenger.png`: `4369D8A20A2E6DCE2760C08160EB64D17B20773647822A8D802BC0D0769D7E61`
- `combat_actor_mill_scavenger.png`: `44E52C0DE6CF4D9289C178458C0A3E6C4740F3B157F42EF2CA83A7110F5866B1`

# Location Scene Sprites 01

- Asset pack: `games/rb-dark-rpg/assets/ui/generated/location_scene_sprites_01/`
- Game: `rb-dark-rpg`
- Role: P0 transparent scene sprites for the current illustrated locations: Last Post hub, gate outskirts route, and Old Mill.
- Status: accepted source sprites for design/runtime wiring.
- Origin: AI-generated raster source sheets sliced into transparent PNG sprites.
- Date: 2026-07-05.
- License: project-internal generated asset.

## Source-First Check

The local shared asset library was searched before generation:

- `node ai_studio/assets/backlog/storage/search.mjs --query "dark rpg blacksmith trader elder healer npc transparent sprite"` -> 0 matches.
- `node ai_studio/assets/backlog/storage/search.mjs --query "dark rpg contract board dragon memorial gate cage prop transparent sprite"` -> 0 matches.
- `node ai_studio/assets/backlog/storage/search.mjs --query "dark rpg scavenger knifeman enemy transparent combat sprite"` -> 0 matches.

No existing local/shared asset matched the current city reference style closely enough for all required NPCs, props, and Old Mill scene objects. The accepted output is therefore generated project art, not sourced third-party art.

## Style Reference

Generation used the accepted city background as the style reference:

- `assets/scenes/last_post_background_candidate05_1280x700.png`

The target style was the game's current city screen: chunky dark-fantasy proportions, painterly low-noise edges, readable silhouettes, dark iron/wood/stone materials, and warm ember accents. The rejected hanging black sun sign from the previous pass is not used as a main building facade prop; the accepted Black Sun clue is a small broken wall fragment for inspection context.

## AI Source Sheets

Generated via the project image-generation helper using the user-approved external image backend:

- Model: `gpt-image-2`
- Quality: `high`
- Requested size: `1536x1024`
- Prompts: `prompt_npc_sheet.txt`, `prompt_props_sheet.txt`, `prompt_mill_sheet.txt`

Source sheets copied into this pack:

- `source_npc_sheet.png`: blacksmith, town trader, elder, healer. SHA-256 `BC501D9CAF611012593598DE1E9A05BB1ACD4118932DC100DEAD3E9EDBC36B18`.
- `source_props_sheet.png`: contract board, Dragon memorial, map gate, caged scavenger. SHA-256 `49AA61BFCE7099A12436268E25DFEB2A189C23ACB02598DEDFC654C7CBD15C2B`.
- `source_mill_sheet.png`: mill scavenger, cellar knifeman, cellar hatch, Black Sun clue wall. SHA-256 `AF11FE2EB0867BC1FE910D5C7464552778F2B879A8755761863DD8F809810D87`.

Cleanup used `tmp/rb_dark_rpg_scene_sprites_prompts/slice_scene_sheets.py`: fixed 2x2 sheet slicing, green chroma-key removal to alpha, alpha-bbox trimming, portrait crops for NPCs, and contact-sheet proof.

## Canvas Handoff

Canvas project: `rb-dark-rpg-p0-scene-sprites-2026-07-05-9109a4`

Accepted image elements:

- Source sheets: `el_bf97a8c1`, `el_e17fcf9c`, `el_a3c3c133`
- Contact sheet: `el_f1c12568`
- Character and portrait sprites: `el_34c3d42c`, `el_0d2bff55`, `el_fcaeaf01`, `el_975ebee5`, `el_ab446703`, `el_65ce27dd`, `el_23419140`, `el_f8b44697`
- Prop/enemy sprites: `el_59d6beb1`, `el_5fde34a2`, `el_eb2db960`, `el_b452671b`, `el_e97c8c88`, `el_b5389bd4`, `el_d2d44690`, `el_4811741a`
- Provenance note: `el_b1ad9d3f`

## Integrity

Per-sprite dimensions and SHA-256 values are recorded in `manifest.generated.json` and mirrored into `design/data/asset_manifest.json` for asset ids that the design data references.

- Contact sheet SHA-256: `53FB6F30776852837A5525C14429DDFC2A7923B8BDAAA3E8E1F96C59179781C6`.

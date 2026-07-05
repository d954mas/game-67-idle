# Equipment Icons 01

- Asset pack: `games/rb-dark-rpg/assets/ui/generated/equipment_icons_01/`
- Game: `rb-dark-rpg`
- Role: AI-generated source sheets sliced into one reusable cell plus transparent slot, gear, and reward item overlay icons.
- Status: placeholder-ready for runtime atlas packing.
- Origin: AI-generated raster source sheets with deterministic crop/key cleanup.
- Generator: `games/rb-dark-rpg/tools/generate_equipment_icons.py`.
- Source data: `games/rb-dark-rpg/design/data/items.json`.
- Date: 2026-07-05.
- Final icon size: 64x64 RGBA PNG.
- License: project-internal generated asset.

## Source-First Check

The local shared asset library was searched before generation:

- `node ai_studio/assets/backlog/storage/search.mjs --query "dark rpg equipment icons weapon armor boots ring relic" --kind item_icon,ui_icon --limit 12 --json` -> 0 matches.
- `node ai_studio/assets/backlog/storage/search.mjs --query "inventory slot icons rpg gear" --kind ui,item_icon,ui_icon --limit 12 --json` -> 0 matches.
- `node ai_studio/assets/backlog/storage/search.mjs --query "Kenney UI icons RPG equipment" --limit 12 --json` -> 0 matches.
- `node ai_studio/assets/backlog/storage/search.mjs --query "dark rpg reward icons xp gold quest clue item tokens" --kind item_icon,ui_icon --limit 12 --json` -> 0 matches.
- `node ai_studio/assets/backlog/storage/search.mjs --query "xp reward token parchment clue sacks dark fantasy inventory icon" --kind item_icon,ui_icon --limit 12 --json` -> 0 matches.

A quick free-source check did not find one reliable CC0/OFL source set that covers all current slots and gear items without mixed-license or visual-style mismatch. The accepted output is therefore generated project art, not sourced third-party art.

## AI Source Sheets

- `cell_source.png`: role `cell`, SHA-256 `2DD41B3CAB03BC2992FD3B402D51B65A9727C4105444B4935C50739A406C33DC`, prompt `prompt_cell.txt`.
- `slots_source_sheet.png`: role `slots`, SHA-256 `132AAFD810CC6943A6A8280E70DFFADC490DAC8B60633B4A7CA964E36767F43D`, prompt `prompt_slots.txt`.
- `gear_source_sheet.png`: role `gear`, SHA-256 `7B4791F29195DCE6479B109887F9182D3F49D5E01B788D31486DEF870843F63A`, prompt `prompt_gear.txt`.
- `reward_source_sheet.png`: role `reward`, SHA-256 `E6FFE86A2465B22D7966CC978399DD5102125D8591A137E3E07C00D0D58184C9`, prompt `prompt_reward_source_sheet.txt`.

Generation path for `reward_source_sheet.png`: built-in `image_gen` tool after Path A was unavailable on this Windows host (`bash`/WSL missing; `codex.exe` access denied from PowerShell).

Canvas handoff for `reward_source_sheet.png`: project `rb-dark-rpg-reward-icon-source-sheet-6d8622`, image element `el_7a738c3b`; prompt/provenance note `el_957e3538`.

Layering contract: `asset_equipment_slot_cell` is the reusable cell/socket. Slot placeholder, gear item, XP token, and reward item icons are transparent overlays rendered above that cell; they are not baked into a rectangle.

Cleanup: source sheets use flat green chroma-key background. The prep script crops each row-major cell, removes the green key to alpha, dampens green edge spill, and downsamples to 64x64.

## Regeneration

```powershell
py -3.12 games/rb-dark-rpg/tools/generate_equipment_icons.py
```

## Integrity

Per-icon SHA-256 values are recorded in `manifest.generated.json` and mirrored in `design/data/asset_manifest.json` for runtime asset ids.

- Contact sheet SHA-256: `9D7696594AA1B5591E274582B92B555E0DB27849544C98C24CCDAC11386380CC`

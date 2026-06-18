---
type: SourceSheetPromptPacket
job_id: mine-cards-equipment-source-v001
source_family: isolated equipment sprite sheet
suggested_key_color: #00ff00
key_color_source: intake_audit
intake_key_color_action: regenerate_with_next_prompt_key_color
---

# Source Sheet Prompt Packet: isolated equipment sprite sheet

## Prompt

Create a production source sheet for Mine Cards production equipment item source sheet. Source family: isolated equipment sprite sheet. Role: cuttable standalone equipment/item sprites: tools, helmet, armor, rings, charm, and relic scroll; no UI frames or text. This is a cuttable source sheet, not a gameplay screenshot, mockup, landing page, or composed UI screen. Arrange the sheet as a row_major_grid with at least 64px outer margin and 48px gutters between all visible pixels and shadows. Row 1 mining_first: worn stone/iron pickaxe, copper pickaxe, mining helmet, and miner chest armor; one isolated component per slot with clear gutters and no composed runtime screen. Slots in order: item_pickaxe_worn, item_pickaxe_copper, item_mining_helmet, item_miner_armor. Row 2 progression_trinkets: plain dark metal ring, ruby ring, skull relic scroll, and mine charm amulet; one isolated component per slot with clear gutters and no composed runtime screen. Slots in order: item_plain_ring, item_ruby_ring, item_skull_relic_scroll, item_mine_charm. Row 3 later_rpg_reference: hand axe, battle axe, spiked mace, and dark crown for later RPG equipment mood; one isolated component per slot with clear gutters and no composed runtime screen. Slots in order: item_hand_axe, item_battle_axe, item_spiked_mace, item_dark_crown. Leave empty chroma lanes between rows and slots. Do not compose these assets into a runtime screen. Use a perfectly flat chroma background #00ff00 or true transparency; do not use gradients, shadows, glow, or texture in the background. Keep every asset isolated with generous gutters and no overlap between shadows. Keep all labels, numbers, quest text, counters, and state values blank for runtime composition. Put unique gems, badges, medallions, locks, banners, and center ornaments in separate overlay sprites, not inside stretchable bases. Use consistent perspective, lighting, material language, and line weight across the sheet.

## Negative Prompt

stat labels, rarity labels, item names, UI frames, button labels, button backgrounds, resource counters, game state values, debug text, cast shadows on the background, random letters, watermarks, labels fused into item art, items fused into UI frames, wrong subject, weak silhouette at 64px, opaque or non-flat background, white/gray background, baked cast shadow, halo or matte fringe, neighboring fragments inside expanded crop, clipped silhouette, copying Minecraft/Steve-adjacent character motifs, readable text, fake letters, watermark, fused icons inside buttons, cropped silhouettes, merged components, busy stretch centers, non-flat chroma background, purple or red-blue halo on transparent edges

## Acceptance Checklist

- Source sheet is not a full gameplay screenshot.
- Background is flat #00ff00 or true transparent alpha.
- No readable text, fake glyphs, labels, numbers, or watermark.
- Components have clear gutters and do not share antialias/shadow pixels.
- No stretchable slice9 bases are mixed into this source family.
- Ornaments that should not stretch are separate overlay sprites with visible isolation.
- Icons/sprites have full silhouettes with padding for alpha trim.
- Source can pass source-sheet intake before crop rectangles are trusted.
- Accepted output will get a generation record with provider/model/workflow/seed or no-seed reason.

## Source Sheet Layout

sheet_role: cuttable_source_sheet
placement: row_major_grid
outer_margin_px_min: 64
gutter_px_min: 48

- row 1 `mining_first`: worn stone/iron pickaxe, copper pickaxe, mining helmet, and miner chest armor; one isolated component per slot with clear gutters and no composed runtime screen; slots: item_pickaxe_worn, item_pickaxe_copper, item_mining_helmet, item_miner_armor
- row 2 `progression_trinkets`: plain dark metal ring, ruby ring, skull relic scroll, and mine charm amulet; one isolated component per slot with clear gutters and no composed runtime screen; slots: item_plain_ring, item_ruby_ring, item_skull_relic_scroll, item_mine_charm
- row 3 `later_rpg_reference`: hand axe, battle axe, spiked mace, and dark crown for later RPG equipment mood; one isolated component per slot with clear gutters and no composed runtime screen; slots: item_hand_axe, item_battle_axe, item_spiked_mace, item_dark_crown

## Intake Routing

recommended_next_step: regenerate_source_sheet_with_safer_key_color
blocking_reasons: key_color_conflict, unsafe_border, unsafe_gutter

## Relevant Asset Groups

- equipment_item_sprites (sprite): isolated equipment and trinket sprites with explicit pivots
- equipment_icon_set (icon): compatibility group for generated-asset validators; actual current source family exports the same equipment items as sprites/icons, not separate UI icons

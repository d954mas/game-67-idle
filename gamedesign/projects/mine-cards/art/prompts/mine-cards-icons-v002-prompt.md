---
type: SourceSheetPromptPacket
job_id: mine-cards-stage-ui-family-v001
source_family: isolated icon sheet
suggested_key_color: #ff00ff
key_color_source: intake_audit
intake_key_color_action: regenerate_with_next_prompt_key_color
---

# Source Sheet Prompt Packet: isolated icon sheet

## Prompt

Create a production source sheet for Mine Cards stage UI and icon source family. Source family: isolated icon sheet. Role: semantic icons with gutters and transparent/chroma source. This is a cuttable source sheet, not a gameplay screenshot, mockup, landing page, or composed UI screen.

Make exactly a 4x4 row-major grid on a perfectly flat solid #ff00ff chroma background. Use 128px minimum outer margin and 128px minimum clear magenta gutters between every visible pixel, shadow, sparkle, and component. Each occupied slot must contain exactly one single connected chunky voxel icon silhouette, centered in its slot. Do not add loose pebbles, sparks, labels, dashed borders, separate tiny fragments, or decorative satellites around any icon. Slot 16 may be a plain empty magenta slot with no visible marks.

Generate these Mine Cards-specific icons in this exact row-major order:

1. activity_mining: mine cart full of gray stone with a small warm lantern
2. activity_woodcutting: blocky tree stump with axe embedded
3. activity_fishing: fishing rod and fish on a small water tile, one connected icon
4. activity_smithing: stone forge/anvil with warm fire
5. activity_combat: crossed sword and shield, one connected icon
6. activity_farming: compact crop plot with carrots and wheat, connected base
7. activity_bank: small stone bank chest/vault building with coin emblem
8. activity_shop: wooden market stall with coin sign and lantern
9. resource_stone: single chunky gray stone boulder, no loose pebbles
10. resource_copper_ore: single chunky brown rock with copper chunks, no loose pebbles
11. resource_coin: single coin stack cluster, connected pile
12. upgrade_pickaxe: stone pickaxe with warm highlight, no glow particles
13. state_locked: chunky padlock
14. state_equipped: backpack or equipment chest
15. state_ready: green check marker on a small wooden plaque, connected icon
16. empty magenta slot for spacing test, no dashed outline

Use chunky voxel-idle style, warm mine lighting, original silhouettes, readable at 24-48px runtime size. Do not use #ff00ff, magenta, purple, or pink inside any icon. Keep all labels, numbers, quest text, counters, and state values blank for runtime composition. Use consistent perspective, lighting, material language, and line weight across the sheet.

## Requested Runtime Asset IDs

Generate these Mine Cards-specific icons, not generic RPG icons:

- activity_mining
- activity_woodcutting
- activity_fishing
- activity_smithing
- activity_combat
- activity_farming
- activity_bank
- activity_shop
- resource_stone
- resource_copper_ore
- resource_coin
- upgrade_pickaxe
- state_locked
- state_equipped
- state_ready

Style: chunky voxel-idle icons, warm mine lighting, original silhouettes, readable at 24-48px runtime size. Avoid Minecraft item shapes and avoid baked labels. Avoid magenta, pink, and purple inside art because #ff00ff is the source key color.

## Negative Prompt

button labels, resource counters, timer values, tutorial text, debug text, game state values, random letters, watermarks, labels fused into reusable backgrounds, icons fused into buttons, wrong subject, weak silhouette at gameplay size, opaque chroma-key background, baked board state, readable text, fake letters, watermark, fused icons inside buttons, cropped silhouettes, merged components, busy stretch centers, non-flat chroma background, purple or red-blue halo on transparent edges

## Acceptance Checklist

- Source sheet is not a full gameplay screenshot.
- Background is flat #ff00ff or true transparent alpha.
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
outer_margin_px_min: 128
gutter_px_min: 128

- row 1: activity_mining, activity_woodcutting, activity_fishing, activity_smithing
- row 2: activity_combat, activity_farming, activity_bank, activity_shop
- row 3: resource_stone, resource_copper_ore, resource_coin, upgrade_pickaxe
- row 4: state_locked, state_equipped, state_ready, empty_magenta_slot

## Intake Routing

recommended_next_step: regenerate_source_sheet_with_safer_key_color
blocking_reasons: key_color_conflict, unsafe_gutter

## Relevant Asset Groups

- ui_icon_set (icon): isolated readable icons with transparent or chroma-key background

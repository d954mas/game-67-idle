---
type: SourceSheetPromptPacket
job_id: mine-cards-stage-ui-family-v001
source_family: isolated icon sheet
suggested_key_color: #00ff00
key_color_source: default
intake_key_color_action: none
---

# Source Sheet Prompt Packet: isolated icon sheet

## Prompt

Create a production source sheet for Mine Cards stage UI and icon source family. Source family: isolated icon sheet. Role: semantic icons with gutters and transparent/chroma source. This is a cuttable source sheet, not a gameplay screenshot, mockup, landing page, or composed UI screen. Arrange the sheet as a row_major_grid with at least 64px outer margin and 64px gutters between all visible pixels and shadows. Row 1 core_gameplay_icons: health, shield, currency, quest, travel, resource, lock/unlock icons; one centered silhouette per slot; no frames fused into icons; readable at gameplay size. Row 2 state_and_resource_variants: rarity/state/resource variants with shared visual language; consistent lighting and padding; no touching shadows between neighboring slots. Leave empty chroma lanes between rows and slots. Do not compose these assets into a runtime screen. Use a perfectly flat chroma background #00ff00 or true transparency; do not use gradients, shadows, glow, or texture in the background. Keep every asset isolated with generous gutters and no overlap between shadows. Keep all labels, numbers, quest text, counters, and state values blank for runtime composition. Create only semantic gameplay/resource icons with strong silhouettes, no frames fused into icons, and enough padding for alpha trim. Put unique gems, badges, medallions, locks, banners, and center ornaments in separate overlay sprites, not inside stretchable bases. Use consistent perspective, lighting, material language, and line weight across the sheet.

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

Style: chunky voxel-idle icons, warm mine lighting, original silhouettes, readable at 24-48px runtime size. Avoid Minecraft item shapes and avoid baked labels.

## Negative Prompt

button labels, resource counters, timer values, tutorial text, debug text, game state values, random letters, watermarks, labels fused into reusable backgrounds, icons fused into buttons, wrong subject, weak silhouette at gameplay size, opaque chroma-key background, baked board state, readable text, fake letters, watermark, fused icons inside buttons, cropped silhouettes, merged components, busy stretch centers, non-flat chroma background, purple or red-blue halo on transparent edges

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
gutter_px_min: 64

- row 1 `core_gameplay_icons`: health, shield, currency, quest, travel, resource, lock/unlock icons; one centered silhouette per slot; no frames fused into icons; readable at gameplay size
- row 2 `state_and_resource_variants`: rarity/state/resource variants with shared visual language; consistent lighting and padding; no touching shadows between neighboring slots

## Intake Routing

recommended_next_step: none
blocking_reasons: none

## Relevant Asset Groups

- ui_icon_set (icon): isolated readable icons with transparent or chroma-key background

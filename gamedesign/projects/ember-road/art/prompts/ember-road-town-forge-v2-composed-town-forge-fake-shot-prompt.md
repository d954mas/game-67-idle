---
type: SourceSheetPromptPacket
job_id: ember-road-town-forge-v2
source_family: composed town forge fake shot
suggested_key_color: #00ff00
key_color_source: explicit_override
intake_key_color_action: none
---

# Source Sheet Prompt Packet: composed town forge fake shot

## Prompt

Create a production source sheet for town forge equipment direction target and source families. Source family: composed town forge fake shot. Role: full-screen target composition for direction review only; not a fused runtime asset. This is a cuttable source sheet, not a gameplay screenshot, mockup, landing page, or composed UI screen. Arrange the sheet as a row_major_grid with at least 64px outer margin and 48px gutters between all visible pixels and shadows. Row 1 isolated_assets: all requested reusable runtime assets for this source family; one isolated component per slot with clear gutters and no composed runtime screen. Leave empty chroma lanes between rows and slots. Do not compose these assets into a runtime screen. Use a perfectly flat chroma background #00ff00 or true transparency; do not use gradients, shadows, glow, or texture in the background. Keep every asset isolated with generous gutters and no overlap between shadows. Keep all labels, numbers, quest text, counters, and state values blank for runtime composition. Put unique gems, badges, medallions, locks, banners, and center ornaments in separate overlay sprites, not inside stretchable bases. Use consistent perspective, lighting, material language, and line weight across the sheet.

## Negative Prompt

button labels, resource counters, timer values, tutorial text, debug text, game state values, unreadable fake letters, exact reference-game UI ornaments, numbers inside cost/result slots, random letters, watermarks, labels fused into reusable backgrounds, icons fused into buttons, wrong subject, weak silhouette at gameplay size, opaque chroma-key background, baked board state, one fused screenshot used as runtime UI, shape-rectangle programmer art, text or labels baked into reusable pieces, Y-up assumptions inverted in layout notes, panel-only forge UX with no scene object anchor, readable text, fake letters, watermark, fused icons inside buttons, cropped silhouettes, merged components, busy stretch centers, non-flat chroma background, purple or red-blue halo on transparent edges

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

- row 1 `isolated_assets`: all requested reusable runtime assets for this source family; one isolated component per slot with clear gutters and no composed runtime screen

## Intake Routing

recommended_next_step: none
blocking_reasons: none

## Relevant Asset Groups

- composed_town_forge_fake_shot (reference_image): single composed target for the town forge upgrade state: forge/workbench in the scene, hero and blacksmith/warden focus, Mine Lantern object/result slot, ember shard cost, route strip showing Depth 2 opened, and persistent browser-RPG frame
- town_forge_scene_layers (background): painted Old Gate forge/town workbench layer, lantern crafting focus, forge glow, and route/depth marker overlays
- forge_lantern_resource_icons (icon): Mine Lantern, ember shard, forge hammer/anvil, unlock/depth, cost paid, equipped result icons
- town_forge_characters (sprite): isolated hero, blacksmith or Gate Warden forge pose, lantern pickup/equip marker, and small forge glow FX
- fantasy_forge_ui_frame (slice9): ornate but readable browser-RPG frame pieces for right equipment/result rail, route strip, bottom log, reward/equipment slots, and primary button bases

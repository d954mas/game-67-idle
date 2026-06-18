---
type: SourceSheetPromptPacket
job_id: mine-cards-stage-ui-family-v001
source_family: sprite or FX sheet
suggested_key_color: #0000ff
key_color_source: explicit_override
intake_key_color_action: none
---

# Source Sheet Prompt Packet: sprite or FX sheet

## Prompt

Create a production source sheet for Mine Cards stage UI and icon source family. Source family: sprite or FX sheet. Role: isolated gameplay sprites/effects with pivots. This is a cuttable source sheet, not a gameplay screenshot, mockup, landing page, or composed UI screen. Arrange the sheet as a row_major_grid with at least 64px outer margin and 48px gutters between all visible pixels and shadows. Row 1 isolated_assets: all requested reusable runtime assets for this source family; one isolated component per slot with clear gutters and no composed runtime screen. Leave empty chroma lanes between rows and slots. Do not compose these assets into a runtime screen. Use a perfectly flat chroma background #0000ff or true transparency; do not use gradients, shadows, glow, or texture in the background. Keep every asset isolated with generous gutters and no overlap between shadows. Keep all labels, numbers, quest text, counters, and state values blank for runtime composition. Put unique gems, badges, medallions, locks, banners, and center ornaments in separate overlay sprites, not inside stretchable bases. Use consistent perspective, lighting, material language, and line weight across the sheet.

## Negative Prompt

button labels, resource counters, timer values, tutorial text, debug text, game state values, random letters, watermarks, labels fused into reusable backgrounds, icons fused into buttons, wrong subject, weak silhouette at gameplay size, opaque chroma-key background, baked board state, readable text, fake letters, watermark, fused icons inside buttons, cropped silhouettes, merged components, busy stretch centers, non-flat chroma background, purple or red-blue halo on transparent edges

## Acceptance Checklist

- Source sheet is not a full gameplay screenshot.
- Background is flat #0000ff or true transparent alpha.
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

- gameplay_sprite_or_marker (sprite): isolated gameplay sprite, marker, or FX with pivot/anchor

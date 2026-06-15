---
type: SourceSheetPromptPacket
job_id: rune-marches-ui-map-rescue-v2
source_family: blank UI kit sheet
suggested_key_color: #00ff00
key_color_source: intake_audit
intake_key_color_action: none
---

# Source Sheet Prompt Packet: blank UI kit sheet

## Prompt

Create a production source sheet for runtime-ui-map-kit. Source family: blank UI kit sheet. Role: stretchable bases: panels, button states, bars, frames, and chips without unique center decoration in slice9 stretch zones. This is a cuttable source sheet, not a gameplay screenshot, mockup, landing page, or composed UI screen. Arrange the sheet as a row_major_grid with at least 64px outer margin and 48px gutters between all visible pixels and shadows. Row 1 large_slice9_bases: large blank panels, journals, modals, inventory frames; one isolated base per slot; blank content area; no center badges or unique stretch-zone ornaments. Row 2 button_and_chip_bases: primary/secondary button bases, chips, tabs, compact controls; separate idle/pressed/disabled/selected states; no baked labels or icons. Row 3 bar_and_strip_bases: progress tracks, status strips, separators, meter frames; split caps and repeatable center strips where ornaments would stretch. Leave empty chroma lanes between rows and slots. Do not compose these assets into a runtime screen. Use a perfectly flat chroma background #00ff00 or true transparency; do not use gradients, shadows, glow, or texture in the background. Keep every asset isolated with generous gutters and no overlap between shadows. Keep all labels, numbers, quest text, counters, and state values blank for runtime composition. For slice9 bases, keep stretch zones structurally boring: corners, straight edges, fill, and repeatable texture only. Put unique gems, badges, medallions, locks, banners, and center ornaments in separate overlay sprites, not inside stretchable bases. Use consistent perspective, lighting, material language, and line weight across the sheet.

## Negative Prompt

button labels, resource counters, timer values, tutorial text, debug text, game state values, quest state, map labels, random letters, watermarks, labels fused into reusable backgrounds, icons fused into buttons, wrong subject, weak silhouette at gameplay size, opaque chroma-key background, baked board state, busy label overlaps, debug-looking button grid, fake-shot crop artifacts, slice9 edge ornament stretches visibly, magenta fringe after chroma removal, readable text, fake letters, watermark, fused icons inside buttons, cropped silhouettes, merged components, busy stretch centers, non-flat chroma background, purple or red-blue halo on transparent edges

## Acceptance Checklist

- Source sheet is not a full gameplay screenshot.
- Background is flat #00ff00 or true transparent alpha.
- No readable text, fake glyphs, labels, numbers, or watermark.
- Components have clear gutters and do not share antialias/shadow pixels.
- Slice9 centers and long edges have no unique ornaments that will stretch.
- Ornaments that should not stretch are separate overlay sprites with visible isolation.
- Icons/sprites have full silhouettes with padding for alpha trim.
- Source can pass source-sheet intake before crop rectangles are trusted.
- Accepted output will get a generation record with provider/model/workflow/seed or no-seed reason.

## Source Sheet Layout

sheet_role: cuttable_source_sheet
placement: row_major_grid
outer_margin_px_min: 64
gutter_px_min: 48

- row 1 `large_slice9_bases`: large blank panels, journals, modals, inventory frames; one isolated base per slot; blank content area; no center badges or unique stretch-zone ornaments
- row 2 `button_and_chip_bases`: primary/secondary button bases, chips, tabs, compact controls; separate idle/pressed/disabled/selected states; no baked labels or icons
- row 3 `bar_and_strip_bases`: progress tracks, status strips, separators, meter frames; split caps and repeatable center strips where ornaments would stretch

## Intake Routing

recommended_next_step: none
blocking_reasons: none

## Relevant Asset Groups

- exploration_ui (slice9): One primary action button, small secondary buttons, journal panel, reward chip, status strips, and selected/disabled/affordable states.
- combat_ui (slice9): Combat-only action buttons and enemy panel; do not keep combat grid visible during exploration.
- ui_decor_overlays (sprite): Separate non-stretch corner caps, gems, badges, medallions, lock plates, ribbon banners, dividers, and small fantasy flourishes with transparent/chroma padding.

---
id: T0018
title: Fix 67 World native HUD readability
status: review
epic: ""
priority: P1
tags: [ui, visual, native, readability]
created: 2026-06-12
updated: 2026-06-12
---

## What

Clean up the native PC top HUD and compact text layout after the field-first
art integration. The field is now visually stronger, but screenshot evidence
shows the top bar is still too dense at 960x540: counters, the title, and the
speed upgrade state compete with panel art and each other.

Do not solve this by switching to web. Keep the PC harness as the validation
surface.

## Done when

- [x] Top HUD counters, title, and speed upgrade text are readable at 960x540.
- [x] Text does not overlap panel borders or nearby controls.
- [x] UI uses reusable runtime pieces or simple native shapes, not baked labels.
- [x] Native scenario screenshot confirms the screen remains playable and
      field-first.
- [x] Pixel health and first-loop DevAPI scenario pass.

## Open questions

None.

## Log

- 2026-06-12: Created after repair screenshot
  `build/captures/scenarios/first_67_loop_field_repair_v2.png`. The art repair
  succeeded, but the top HUD still needs a dedicated readability pass.
- 2026-06-12: Replaced fixed-coordinate top HUD text with explicit HUD boxes
  for coins, collection, title, and speed state in `src/main.c`. Native evidence:
  `build/captures/scenarios/first_67_loop_hud_readability_v3.png`; first-loop
  DevAPI scenario and pixel health passed.

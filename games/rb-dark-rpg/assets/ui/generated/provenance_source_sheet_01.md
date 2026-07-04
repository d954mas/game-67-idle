---
type: Asset Provenance
game_id: rb-dark-rpg
asset_family: ui_generated
origin: ai
tool: openai image generation via nt-asset-image-generation generate_image.py
license: project-internal generated asset
source_image: source_sheet_01.png
sha256: 6A08459A5C08E1C95D9F1BBD27EF12CBC459AF5F01532AE94C4D8D165A20AC04
generated_at: 2026-07-04
---

# UI Source Sheet 01 Provenance

Purpose: production-direction source sheet for `rb-dark-rpg` dark fantasy fade
HUD UI components.

Prompt:

```text
Production-ready dark fantasy browser RPG UI source sheet, real raster image not code-drawn. 1536x1024 source sheet on a flat matte chroma green background (#00ff00) up to every object edge, NO text, NO letters, NO numbers, NO icons, NO logos. Style: Roblox-like blocky dark fantasy, old browser RPG, blackened bronze, aged leather, subtle gold trim, readable chunky forms, low-detail, game UI usable at 960x540. Include separated empty reusable UI parts with spacing: top HUD character frame, location plaque frame, resource chip frame, bottom navigation button normal state, selected state, locked/disabled state, compact bottom sheet panel, list row normal, list row active, list row locked, small quest marker frame, HP bar empty frame, XP bar empty frame, small square utility button frame. No baked fill, no content inside, no drop shadows beyond small contact occlusion, clean edges for cutout.
```

Accepted derivatives:

- `hud_player_frame.png`
- `location_plaque.png`
- `resource_chip.png`
- `nav_button_normal.png`
- `nav_button_selected.png`
- `nav_button_locked.png`
- `bottom_sheet_panel.png`
- `list_row_normal.png`
- `list_row_active.png`
- `list_row_locked.png`
- `square_utility_frame.png`
- `bar_frame_small.png`
- `bar_frame_large.png`
- `quest_marker_frame.png`

Notes:

- Runtime text, icons, fills, and values must remain engine-rendered.
- The generated sheet was chroma-keyed and spill-suppressed before cropping.
- These assets establish the current art direction; atlas/slice9 metadata is
  still needed before runtime integration.

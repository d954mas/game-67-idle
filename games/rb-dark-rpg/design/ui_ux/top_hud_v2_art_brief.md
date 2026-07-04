# Top HUD V2 Art Brief

Date: 2026-07-04

Accepted direction:
- Keep top-left empty for Poki reserve.
- Center: compact location plaque for runtime text `Последний Пост`.
- Right: compact player/status/resources/settings cluster, aligned from the right edge.
- Surface treatment: dark iron, worn leather, soot wood, muted brass accents.
- Shape language: chunky, angular, readable Roblox-like fantasy, not glossy MMO.

Generated source:
- `games/rb-dark-rpg/assets/ui/generated/top_hud_tokens_02/top_hud_component_sheet_02.png`

Components to slice:
- `top_hud_portrait_frame`
- `top_hud_status_panel`
- `top_hud_bar_hp`
- `top_hud_bar_xp`
- `top_hud_resource_gold`
- `top_hud_resource_supplies`
- `top_hud_location_plaque`
- `top_hud_settings_button`
- `top_hud_icon_gold`
- `top_hud_icon_supplies`
- `top_hud_level_badge`

Implementation notes:
- Do not bake Russian labels into art.
- Draw HP/XP fills in runtime inside empty generated frames.
- Use generated components over the existing top fade band.
- Replace the visible `Опции` text button with a compact generated gear button if implementation scope allows it cleanly.

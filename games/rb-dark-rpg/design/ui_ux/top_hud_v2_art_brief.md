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

Sliced runtime components:
- `top_hud_portrait_frame`
- `top_hud_status_plaque`
- `top_hud_hp_frame`
- `top_hud_xp_frame`
- `top_hud_resource_coin_chip`
- `top_hud_resource_supplies`
- `top_hud_location_plaque`
- `top_hud_settings_button`
- `top_hud_icon_coin`
- `top_hud_icon_supplies`
- `top_hud_level_badge`

Implementation notes:
- Do not bake Russian labels into art.
- Draw HP/XP fills in runtime inside empty generated frames.
- Use generated components over the existing top fade band.
- Implementation status: integrated into `first_screen_hud.c` and `sys_settings.c`; verified through `quality_responsive` with evidence at `tmp/quality/qclr_002_responsive/contact_sheet.png`.
- Replace the visible `Опции` text button with a compact generated gear button if implementation scope allows it cleanly.

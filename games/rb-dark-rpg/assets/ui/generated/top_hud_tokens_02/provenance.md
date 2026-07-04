# Top HUD Tokens 02 Provenance

Asset: `top_hud_component_sheet_02.png`

Status: accepted by art lead and subagent review; integrated into runtime top HUD.

Source:
- Generated with built-in Codex image generation on 2026-07-04.
- Original generated file: `C:\Users\ROG\.codex\generated_images\019f2c62-4c25-7b41-8b29-75c27712dc18\ig_08f2cec7d5422f2b016a49191a3308819196b39c3832d3bc52.png`
- Workspace copy: `games/rb-dark-rpg/assets/ui/generated/top_hud_tokens_02/top_hud_component_sheet_02.png`
- SHA256: `954CDE18EAB3781DBB6C50DFA45DD42B83BF94E89DACED6B37605D87B4639B11`

Generation brief:
- Low-noise 2D illustrated top-HUD token sheet for `rb-dark-rpg`.
- Dark Roblox-like fantasy, rugged border-city/garrison materials.
- Separate crop-friendly components on flat `#00ff00` chroma-key background.
- No text, numbers, runes, premium gems, mail/social/clan icons, full-width panels, or complete HUD mockup.

Runtime intent:
- Use the sheet only as source art for cropped transparent PNG tokens.
- Runtime owns all labels, values, HP/XP fill amounts, hit testing, and responsive layout.
- Top UI remains floating components over scene fade, not a hard opaque header.
- Runtime evidence: `tmp/quality/qclr_002_responsive/contact_sheet.png`.

Do not use:
- The full sheet directly in-game.
- Static bar fills or baked values.
- Any generated component that conflicts with the Poki top-left reserve.

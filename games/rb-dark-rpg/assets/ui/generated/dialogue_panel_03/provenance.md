# Dialogue Panel 03

- Game: `rb-dark-rpg`
- Role: component sheet and sliced UI pieces for the first-quest dialogue modal.
- Origin: generated project asset for this game.
- Generator: Antigravity image generation (`agy`), model `gemini-imagen`.
- Date: 2026-07-04.
- Source sheet: `source_sheet_03.png`.
- Source SHA256: `20f97e23f7f8af2267977878b8d53896b391eb2664c743f22ecea2cdad459bc8`.
- Source size: 1536x1024, 1,266,500 bytes.
- Canvas import: project `rb-dark-rpg-9874a1`, source group `grp_ab65605a`, source element `el_b8836d99`.
- Canvas slicing: padded regions from `tmp/dialogue_panel_03_canvas_regions_padded.json`, slice group `grp_0f9d6a10`.
- Canvas slice elements: `dialogue_outer_frame` `el_11bdb233`, `dialogue_body_panel` `el_2614ea1f`,
  `dialogue_header_plaque` `el_52c60ffc`, `dialogue_portrait_frame` `el_bc7549f8`,
  `dialogue_objective_panel` `el_2f60038b`, `dialogue_reward_cell` `el_7fef56db`,
  `dialogue_answer_normal` `el_cbb00ff4`, `dialogue_answer_primary` `el_f5f7a497`,
  `dialogue_divider` `el_121e0feb`.
- Alpha cleanup: Canvas `alpha --method matte` on all padded slices. Padding keeps real green
  background around the component, so the keyer uses `[5,250,3]` instead of dark edge pixels.
  Final PNGs are trimmed by alpha bbox after keying so slice9 uses tight components without
  transparent padding. A final hue cleanup neutralizes remaining olive-green edge spill while
  preserving warm bronze/gold borders.
- Prompt: `prompt_v3.txt`.
- References: accepted bottom navigation v11, top HUD tokens, guard portrait, location background, and the rejected dialogue screenshot as a negative layout reference.
- License: generated for this project; do not treat as third-party stock art.

Sliced through `ai_studio/assets/canvas/cli.mjs`; game slices were copied from
`tmp/dialogue_panel_03_canvas_export_final_art/`.

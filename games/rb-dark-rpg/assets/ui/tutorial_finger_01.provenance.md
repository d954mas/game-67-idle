# tutorial_finger_01.png provenance

- Asset: `assets/ui/tutorial_finger_01.png`
- Purpose: FTUE tap prompt for the first `talk to guard` objective.
- Format: 256x256 PNG, transparent background, premultiplied-alpha-safe resize.
- Origin: project-generated raster asset for `rb-dark-rpg`.
- License status: generated specifically for this project; no third-party binary source is included.
- Integrity: SHA256 `C45F4E1EC649E0B6BDB3E42F794E5CCE5773497005E15088EF31203B077F1266`.

## Source search

- Shared asset backlog search: `tutorial finger` returned 0 matches.
- Local engine/source assets inspected and rejected as unsuitable for the dark RPG FTUE prompt:
  - `external/neotolis-engine/assets/sprites/bigatlas/hand.png`
  - `external/neotolis-engine/assets/sprites/bigatlas/hand_.png`
  - `external/neotolis-engine/assets/sprites/bigatlas/hand_token_open.png`
  - `external/neotolis-engine/assets/sprites/bigatlas/card_tap.png`
  - `external/neotolis-engine/assets/sprites/bigatlas/card_tap_down_.png`

## Generation

- Tool: `.codex/skills/nt-asset-image-generation/scripts/generate_image.py`
- Model: `gpt-image-2`
- Size: `1024x1024`
- Quality: `high`
- Source output: `tmp/rb_dark_rpg_tutorial_finger_source_v4.png`
- Source generation hash: `29a7d7ec17496f4b0fdc66d3bcf699feadc9e7cc730a50521b37f61a3e1e8fc7`
- Prompt:

```text
dark fantasy RPG mobile FTUE tutorial hand pointer UI icon, hand enters from lower right and index finger points diagonally up-left at a 35 to 45 degree angle, clearly an interface pointer not an in-world character, ivory parchment-gloved hand with thick dark brown outline and subtle warm gold rim, ornate medieval cuff, simple bold silhouette, readable at 64px, old browser RPG ornate HUD feel, no circle, no ring, no ripple, no tap target, no arrow, no text, no panel, no phone, centered single asset on flat pure green chroma key background
```

## Processing

- Matte cutout: `ai_studio/assets/canvas/tools/alpha_cutout.py`
- Cutout report: `tmp/rb_dark_rpg_tutorial_finger_alpha_report_v4.json`
- Key color: `[4, 248, 10]`
- Cutout route: `key_matte`
- Dual matte required: `false`
- Resize/crop: `tmp/rb_dark_rpg_prepare_tutorial_finger_v4.py`
- Final crop rule: alpha bbox over threshold 8, 32 px padding, square canvas, resize to 256x256.

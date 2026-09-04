# Template UI art (design-system kit)

One slice9 set for the whole interface: rim 3 px + lift 5 px, drawn from the
`--ui-*` tokens in `design/references/ui_kit/styles.css`. Grayscale art is
tinted at runtime — fill = white becomes the action colour, rim and lift = 72%
gray become that colour's deep step; art with a fixed role carries real token
colours. Every game copied from the template starts with this kit; repaint the
tokens and regenerate rather than editing the PNGs.

| File                  | What                             | colours     | slice9 (build_packs.c) |
|-----------------------|----------------------------------|-------------|------------------------|
| `panel.png`           | window / panel frame             | shell+panel | 14 px                  |
| `button.png`          | button with bottom lift          | grayscale   | L/R/T 16 px, B 22 px   |
| `tile.png`            | light item / card surface        | tile+rim    | 14 px                  |
| `slider_track.png`    | recessed track                   | inset+rim   | 11 px                  |
| `slider_fill.png`     | bar fill pill                    | grayscale   | 11 px                  |
| `slider_track_sm.png` | design-size track for the engine slider | inset+rim | 11 px (source pixels) |
| `slider_fill_sm.png`  | design-size fill for the engine slider  | grayscale | 11 px (source pixels) |
| `slider_thumb.png`    | thumb / milestone dot            | grayscale   | (circle, none)         |
| `icon_play.png`       | play glyph for a video badge     | white       | none                   |

The slice9 borders above are design units; the kit ships at 4 source pixels per
design unit, and `src/build_packs.c` multiplies by that scale. The two design-size
copies exist because the engine slider bakes its borders at source-pixel size
(neotolis-engine#349) while everything drawn through an image style scales them
down by `UI_KIT_SLICE9_SCALE`.

## Provenance

- **Origin:** generated (project-original, no third-party inputs)
- **How:** `node ai_studio/dev_environment/python_run.mjs tools/gen_ui_kit.py`
  draws every file at 4x from the design tokens and downsamples with LANCZOS.
  Regenerate after a token change instead of editing PNGs, then refresh the
  `sha256` rows in `assets/packs/template-ui-kit/assets.jsonl`.
- **License:** CC0 1.0 — commercial use, modification and redistribution
  allowed, no attribution required.
- **Integrity and records:** `assets/packs/template-ui-kit/`.

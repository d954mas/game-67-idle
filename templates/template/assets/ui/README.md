# Generated UI kit

These project-original UI PNGs are generated from the shared token sheet at
`features/ui-kit/tokens/studio_default.json`. The owning generator is
`features/ui-kit/tools/gen_ui_kit.py` in the Studio repository.

The standard sheet uses export scale 4, working supersample 16 and BOX area
downsampling. The slider thumb and small slider variants keep their design-size
exports; the play glyph uses half export scale. Colors, radii, slice9 borders
and final dimensions are unchanged by the antialiasing update.

Regenerate from the Studio root with:

```sh
python features/ui-kit/tools/gen_ui_kit.py --out <consumer>/assets/ui
```

Use `--tokens <sheet.json>` for a game-owned sheet. Refresh the SHA-256 and byte
counts in `assets/packs/template-ui-kit/assets.jsonl` after generation.
The pack records license, provenance and origin; existing project-original
CC0-1.0 licensing remains unchanged. Do not hand-edit the generated PNGs.

The runtime uses a premultiplied UI atlas and premultiplied sprite/text blending.
Generated mipmaps require trilinear minification; magnification stays linear.

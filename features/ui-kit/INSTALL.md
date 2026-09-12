# Install: ui-kit

In-place module. One copy lives here; a template or game compiles it from
`${GAME_REPO_ROOT}/features/ui-kit` against its own atlas.

## 1. Compile it

In the consumer's `CMakeLists.txt`:

```cmake
set(UI_KIT_DIR "${GAME_REPO_ROOT}/features/ui-kit")
set(UI_KIT_INC "${UI_KIT_DIR}/include")
set(UI_KIT_SRC "${UI_KIT_DIR}/src")

target_sources(${GAME_TARGET} PRIVATE
    "${UI_KIT_SRC}/ui_tokens.c"
    "${UI_KIT_SRC}/ui_theme.c"
    "${UI_KIT_SRC}/ui_metrics.c"
    "${UI_KIT_SRC}/ui_kit.c"
    "${UI_KIT_SRC}/ui_reach.c"
    "${UI_KIT_SRC}/ui_safe_area.c")
target_include_directories(${GAME_TARGET} PRIVATE "${UI_KIT_INC}")
target_compile_definitions(${GAME_TARGET} PRIVATE FEATURE_UI_KIT=1)
```

## 2. Draw the art

```
node ai_studio/dev_environment/python_run.mjs features/ui-kit/tools/gen_ui_kit.py --tokens features/ui-kit/tokens/studio_b.json --out assets/ui
```

The sheet named here and the preset bound in step 4 are one decision made
twice: panel and tile colours are baked into the PNGs, so art from one sheet
under the styles of another is a repaint that only half happened. Omitting
`--tokens` draws `studio_default.json`, the legacy template look.

That writes ten PNGs: `panel`, `button`, `tile`, `header`, `slider_track`,
`slider_fill`, `slider_track_sm`, `slider_fill_sm`, `slider_thumb`,
`icon_play`. Record them in
the consumer's asset pack manifest with licence, provenance, origin and a
`sha256` per file, and re-run this after any token change. The legacy default sheet
uses working supersample 16 and BOX area downsampling; export scale and slice9
geometry are independent of this offline quality setting. `art.resample` can
explicitly select another Pillow resampling filter.

The glyph set is not generated: `features/ui-kit/assets/icons/<name>.png` are
white masks (CC0, provenance in `assets/icons/README.md`) the pack builder
reads in place. A game with its own glyphs packs same-named masks of its own.

## 3. Pack them

The pack builder stays game code — the feature never writes to a pack. Add the
ten files to the `ui` atlas with the slice9 borders from the token sheet's
`art.slice9`, multiplied by `art.export_scale`, and give the atlas mipmaps: the
kit ships above its on-screen size, so without mips it aliases. Use
`NT_TEXTURE_DEFAULT_FILTER_LINEAR_MIPMAP_LINEAR` for minification and
`NT_TEXTURE_DEFAULT_FILTER_LINEAR` for magnification when generating mipmaps.
Preserve mip-safe gutters and premultiply the atlas during preparation.

`slider_track_sm` / `slider_fill_sm` take their borders at DESIGN size, without
the export multiplier: the engine slider bakes its slice9 at source-pixel size
(neotolis-engine#349) while everything drawn through an image style scales the
borders down by `tokens->slice9_scale`.

Pack the glyphs as plain sprites named `<prefix>/<name>` for every name of
`ui_icons.h` (`UI_KIT_ICONS`, `ui_icon_name`), as the template does under
`kit/`; the builder includes that header, so a new glyph in the kit is packed
without a game edit.

## 4. Bind it

One game-owned file resolves the regions and hands them over, because the
generated asset ids are the game's:

```c
#include "features/ui_kit/ui_theme.h"
#include "features/ui_kit/ui_tokens.h"
#include "generated/game_assets.h"

ui_theme_art_t art = {
    .panel = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_PANEL.value),
    /* ...the other nine... */
};
ui_theme_art_bind_icons(&art, atlas, "kit"); // the glyph set, by name
ui_theme_init(ui_tokens_studio_b(), &art);
```

The supplied sprite shader expects a premultiplied atlas and premultiplies
vertex tint alpha. Bind its material with `nt_blend_alpha_premultiplied()`.
The Slug text shader also outputs premultiplied RGB, so screen text and world
text materials need that blend state too. Applying straight-alpha blending to
these outputs multiplies coverage twice and darkens edges and faded states.
Shaders that output straight alpha need their corresponding straight blend;
this is a contract check, not a global replacement of every alpha material.
Legacy engine descriptors can use `NT_BLEND_MODE_ALPHA` for premultiplied
blending already. Inspect the pinned renderer factors before translating enum
calls to the current blend-state helpers.

## 5. Open a frame

The consumer's UI runtime hands the feature the framebuffer it is about to draw
into, and lays out on the canvas it gets back:

```c
const UiScaleFit fit = ui_frame_begin(fb_w, fb_h, g_nt_window.dpr);
nt_ui_begin(ctx, fit.logical_w, fit.logical_h, dt, pointers, NT_INPUT_MAX_POINTERS);
```

The canvas comes from the sheet: its reference in authored units spans the
available short edge, and that is the only size contract there is. It is a
reference, not a minimum window size — nothing floors or caps the scale. Every
screen reads `ui_metrics()`: text, hits and spacing are authored units, and
`panel_w` is a share of the available width. `ui_css()` keeps its name as the
spelling for "this number is an authored size" and converts nothing. Safe-area
insets stay physical CSS pixels and are converted separately.

Pick `ref_short` from the hand: at that reference the `hit` token has to stay 44
CSS pixels on a 390-pixel phone, which is what `test_ui_scale` checks. Nothing
else enters the size — a window of a given size draws the same interface on a
phone and on a monitor, so resizing a desktop window is a faithful preview of a
phone.

`ui_metrics()` reports `portrait` and `in_hand`, and neither changes a size. Use
them for arrangement: stack a row, move a control under the thumb, replace a
hover-only affordance with a visible button, leave a little more room around
something a finger has to hit. `UI_KIT_IN_HAND=1` forces the touch answer on a
development machine. Dialogs need a content-width dropdown override and bounded,
scrollable content; the template settings screen shows both and keeps Close
outside the scroll region.

## 6. Build a screen

Start from the composites, not the plates: `ui_kit_button` in a sized element,
`ui_kit_counter` for a wallet, `ui_kit_sheet_begin` for a dialog, the
`ui_kit_*_row` functions for settings. `example/native/src/scenes/` holds one
worked screen of each kind; `example/native/SCENES.md` maps them.

Two rules the composites carry: a plate (`ui_kit_panel_begin`, `ui_kit_tile_begin`)
takes no `.id` in its declaration, so wrap it when the ui.tree needs a name;
and a sheet is declared at root level, never inside a scroll or clip. An
engine slider inside a scroll inside a sheet trips a walker assert (its thumb's
clip marker resolves before the sheet's root is laid out), which is why the
studio's settings screen is a scrimmed panel with a scroll and not a sheet.

## 7. Localized text

The kit's text entry points take `const char *`. A consumer with a localization
wrapper adds the bridge to that wrapper's own file, keeping one place where its
string type becomes a raw pointer:

```c
void loc_kit_label(nt_ui_context_t *ctx, LocStr text, const nt_ui_label_style_t *style) {
    ui_kit_label(ctx, text_of(text), style);
}
```

## Verify

```
ctest --test-dir <consumer build dir> -R test_ui_scale --output-on-failure
node --test features/ui-kit/tests/tokens_parity.test.mjs features/ui-kit/tests/icons_manifest.test.mjs
```

Then shoot the consumer's own layout evidence at phone sizes — a canvas rule is
proven by frames, not by asserts:

```
node ai_studio/dev_environment/python_run.mjs <consumer>/devapi/responsive_viewports.py --exe <exe> --out tmp/ui
```

The call above selects Studio B for a new UI polygon/example. Existing template
and game bindings continue to call `ui_tokens_studio_default()` until their
owners deliberately select Studio B and regenerate its matching art.

## Repaint

Copy `tokens/studio_b.json` or `tokens/studio_default.json`, edit it, pass the matching `ui_tokens_t` to
`ui_theme_init`, and regenerate with `--tokens <your sheet>`. See README
"Extension points".

## Uninstall

Drop the six sources and the include directory from the consumer's CMake,
delete its `theme.c` binding, and remove the nine PNGs and their manifest rows.
Nothing else in the feature reaches into a consumer.

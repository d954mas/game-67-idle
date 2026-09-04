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
    "${UI_KIT_SRC}/ui_safe_area.c")
target_include_directories(${GAME_TARGET} PRIVATE "${UI_KIT_INC}")
target_compile_definitions(${GAME_TARGET} PRIVATE FEATURE_UI_KIT=1)
```

## 2. Draw the art

```
node ai_studio/dev_environment/python_run.mjs features/ui-kit/tools/gen_ui_kit.py --out assets/ui
```

That writes nine PNGs: `panel`, `button`, `tile`, `slider_track`, `slider_fill`,
`slider_track_sm`, `slider_fill_sm`, `slider_thumb`, `icon_play`. Record them in
the consumer's asset pack manifest with licence, provenance, origin and a
`sha256` per file, and re-run this after any token change.

## 3. Pack them

The pack builder stays game code — the feature never writes to a pack. Add the
nine files to the `ui` atlas with the slice9 borders from the token sheet's
`art.slice9`, multiplied by `art.export_scale`, and give the atlas mipmaps: the
kit ships above its on-screen size, so without mips it aliases.

`slider_track_sm` / `slider_fill_sm` take their borders at DESIGN size, without
the export multiplier: the engine slider bakes its slice9 at source-pixel size
(neotolis-engine#349) while everything drawn through an image style scales the
borders down by `tokens->slice9_scale`.

## 4. Bind it

One game-owned file resolves the regions and hands them over, because the
generated asset ids are the game's:

```c
#include "features/ui_kit/ui_theme.h"
#include "features/ui_kit/ui_tokens.h"
#include "generated/game_assets.h"

const ui_theme_art_t art = {
    .panel = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_PANEL.value),
    /* ...the other eight... */
};
ui_theme_init(ui_tokens_studio_default(), &art);
```

## 5. Open a frame

The consumer's UI runtime hands the feature the framebuffer it is about to draw
into, and lays out on the canvas it gets back:

```c
const UiScaleFit fit = ui_frame_begin(fb_w, fb_h, g_nt_window.dpr);
nt_ui_begin(ctx, fit.logical_w, fit.logical_h, dt, pointers, NT_INPUT_MAX_POINTERS);
```

Every screen then reads `ui_metrics()` and states no size of its own.

## 6. Localized text

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
node --test features/ui-kit/tests/tokens_parity.test.mjs
```

Then shoot the consumer's own layout evidence at phone sizes — a canvas rule is
proven by frames, not by asserts:

```
node ai_studio/dev_environment/python_run.mjs <consumer>/devapi/responsive_viewports.py --exe <exe> --out tmp/ui
```

## Repaint

Copy `tokens/studio_default.json`, edit it, pass the matching `ui_tokens_t` to
`ui_theme_init`, and regenerate with `--tokens <your sheet>`. See README
"Extension points".

## Uninstall

Drop the five sources and the include directory from the consumer's CMake,
delete its `theme.c` binding, and remove the nine PNGs and their manifest rows.
Nothing else in the feature reaches into a consumer.

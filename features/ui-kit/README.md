# UI Kit

The studio's interface system: the rule that decides how large the interface is,
the art that draws it, the styles that colour it, and the widgets a screen is
built from.

## Purpose

Every game needs the same four things before it can show a dialog, and every
game used to reinvent them: a canvas rule that survives a phone, a slice9 kit, a
theme, and a widget layer. This pack owns all four, so a new prototype starts
with an interface that is already readable in a hand and already looks like one
product.

**Sizing.** One rule, and it is a proportion: the available short edge, after
physical safe-area insets, fits the sheet's reference in authored units. Text,
hits, spacing and panel dimensions are shares of that area, so the interface
owns the same fraction of any window, and rotating the device changes nothing --
the short edge is the same edge in both orientations. There are no minimum
sizes, no density floor and no logical cap: a floor hands a small window a
LARGER share of itself than a big one, which is how a HUD ends up eating a short
window.

The viewport is the ONLY input. Not the device, not the input method, not the
orientation: the same window draws the same interface everywhere, which is what
makes a phone's layout something a reviewer can look at on a monitor by resizing
one.

Pick `ref_short` from the hand, because the finger is the constraint nothing
else can relax: `hit / ref_short * 390 >= 44` keeps a touch target 44 CSS
pixels on a 390-pixel phone. A recommendation the sheet has to satisfy, checked
by eye, not an assert. Every larger screen is
then served by the same number, since a bigger screen makes the same share
physically larger.

`ui_metrics()` reports two facts that change NO size: `portrait` and `in_hand`.
They are for what a size cannot express -- a row that becomes a column, a
control that moves under the thumb, a hover-only affordance that has to become a
visible button, a little more room around something a finger has to hit.

The default look is the studio's, taken from the reference game the lead uses to
set the bar: a prototype should not have to design a UI before it has a game.

## Public surface

- `ui_tokens.h` — `ui_tokens_t`: colour, type ramp, geometry, canvas rule.
  `ui_tokens_studio_default()` preserves the legacy template look;
  `ui_tokens_studio_b()` is the opt-in game-facing preset for new UI examples.
- `ui_scale_policy.h` — `ui_scale_fit()` and `ui_scale_css_unit()`. Pure
  arithmetic, no engine types, so the sizing rule is testable without a
  window.
- `ui_metrics.h` — `ui_frame_begin()` (the consumer's UI runtime opens the
  frame), then `ui_metrics()`, `ui_css()`, `ui_css_unit()`.
- `ui_theme.h` — `ui_theme_init(tokens, art)` and the mutable `g_ui_theme`
  carrying every engine style, the `dropdown` pick-one style among them (a
  screen calls the engine's `nt_ui_combo_*` with it and sets `min_width` to its
  own content box). The CONSUMER resolves the atlas regions, because it owns its
  pack builder and generated asset ids.
- `ui_kit.h` — the widgets in two tiers. Primitives: panel, tile, scrim, label,
  shadowed label, button plate, meter, slider style, touch-target height, and
  the `UI_LAYER_*` order (one surface layer, so nested surfaces stack by tree
  order; then icon, text shadow, text). Composites: a labelled or icon
  button by role (`ui_kit_button`, `ui_kit_icon_button`), the round close
  (`ui_kit_close_button`, the thumb art on the neutral tile with the close
  glyph in ink), the tinted
  plate and disc containers (`ui_kit_plate_begin`, `ui_kit_disc_begin`: a
  socket, a swatch, a badge, where a Clay rectangle would draw an unantialiased
  border), a counter, a captioned meter, a badge, a titled plate
  (`ui_kit_dialog_begin`: the header band, the title on it, the close on the
  corner, the body), a sheet (that plate inside the engine modal), settings
  rows (toggle, checkbox, radio, slider, dropdown) that express the engine's
  stateful widgets in the kit's own art, a segmented pick-one row with every
  option on the line (`ui_kit_choice_row`), the theme's scroll bar, and
  `ui_kit_color`.
- Looks: the theme's default shapes as styles a game starts from
  (`ui_kit_look_panel`, `_tile`, `_button`, `_header`, `_track`, `_fill`,
  `_disc`) and composes with `ui_kit_panel_begin_styled` /
  `ui_kit_radial_begin_styled`. `ui_kit_button_begin_styled` takes a
  `ui_kit_button_look_t`: the idle plate, an optional disabled plate (else the
  theme's `off` fill) and a `reach` the hit box grows to, so a short plate
  meets a finger; pressed comes from `ui_kit_look_state` (an outside lip sinks
  as one shape and the content sinks with it, or the bottom darkens when the
  lip is inside). `ui_kit_dialog_look()` plus `ui_kit_dialog_begin_styled` /
  `ui_kit_sheet_begin_styled` give a dialog its own plate, a header band
  (height, gradient, `checker_px`, dropped while the body scrolls with
  `calm_scroll`), a title inset, a round close or a square one sized and placed
  in the band, and a rule under the band; that body scrolls under the band when
  last frame's content is taller than the safe area leaves it, with half a
  touch height of hysteresis so the scissor never flickers, and a body that
  fits neither scrolls nor clips. The rows take looks too:
  `ui_kit_slider_look_t` (track, fill, knob, caption and value columns),
  `ui_kit_choice_look_t` (option and picked buttons, label column, option
  height) and `ui_kit_dropdown_look_t` for `ui_kit_dropdown_row_styled`
  (trigger, arrow plate, list plate and rows, a mark on the picked row, a list
  that opens above). Choice segments and
  dropdown rows are named `<id>/<i>` and the dropdown trigger `<id>/current`,
  so a DevAPI bot clicks them by name. Looks draw as shapes under either theme,
  so they need the uber material and `ui_kit_shape_bind`.
- `ui_icons.h` and `assets/icons/` — the glyph set: white masks (CC0, see
  `assets/icons/README.md`) named once in an X-macro, so the enum, the pack
  builder's region names and `ui_theme_art_bind_icons` cannot drift. A game
  packs the files under one prefix and binds them in one call; a game with its
  own glyphs replaces any file with a same-named mask. `ui_kit_icon_ref` and
  `ui_kit_icon_tinted` draw one in any colour; `ui_kit_icon_rotated` also turns
  it by a Y-up counter-clockwise angle in radians, as the radial style measures
  angles.
- `example/native/` — the UI Lab: a standalone engine consumer with seven
  scenes and five themes (one of them the shape theme with no art) where every
  widget above is tried in the real renderer, and
  the example a game copies a screen from. Opt-in; nothing in it is compiled
  into a game or the template.
- `ui_safe_area.h` — the device's own insets, in CSS pixels.
- `ui_world_anchor.h` and `ui_screen_pack.h` — text standing over the world.
  `ui_world_to_canvas` and `ui_figure_box` put a world point, or an upright
  figure's box, on the y-down canvas through the view_proj the frame drew
  with, and report false behind the camera. `ui_screen_pack_first_free` picks
  the first of a fixed list of offsets whose box lies inside the bounds and
  clear of every taken box; a fixed list, not a push, so a crowd spreads into
  a patch instead of a column. Both are pure arithmetic with no kit state.
- `ui_shape.h`, `ui_shape_pack.h` and `shaders/ui_uber.vert|frag` — the uber
  sprite material and the shapes it draws without art: rounded panels
  (per-corner radii, two-colour vertical gradient, contour, lip, press, gloss
  band, checker, inner highlight) and discs, rings and arcs (rim, gradient,
  lip, round gloss). Mode 0 of the shader is `sprite.frag`, so a game that
  makes it the context's base material — `nt_ui_set_sprite_material(ctx, uber)`
  with the material created from `ui_kit_uber_material_desc()` and
  `ui_kit_shape_bind(uber, atlas)` — draws icons, Clay rectangles and shapes in
  one draw per text-free run. The desc sets all-zero attr defaults, which every
  plain emit bakes and the shader reads as mode 0. Shapes are GEOMETRY-mode quads against the white
  region (the walker aligns them to four vertices), so they need no atlas
  region; styles are plain structs packed at call time
  (`ui_kit_pack_panel`/`ui_kit_pack_radial`). The consumer packs both shaders
  with its `common/globals.glsl` on the include path and raises the sprite
  renderer's `custom_max_vertices` to `max_vertices`: every UI batch now stages
  under that cap, and a lower one splits a run into extra draws.
  `ui_kit_panel_begin_styled`/`ui_kit_radial_begin_styled` are containers:
  the engine has no container form of `nt_ui_image_custom`, so they open the
  element with the public image payload themselves.
- The shape theme: with `ui_theme_art_t.shapes` set (and the uber material as
  the base), the kit's own plates -- `ui_kit_panel_begin`, `ui_kit_tile_begin`,
  the dialog's header band, every `ui_kit_button*` and the round close,
  `ui_kit_plate_begin`, `ui_kit_disc_begin` and `ui_kit_meter` -- draw as SDF
  shapes of what `gen_ui_kit.py` bakes: the same radii (`r_panel`, `r_button`,
  `r_tile`, `r_bar`, `r_header` in the tokens), rim, lift and ledge shade
  (`ledge_step`), in the UI units the art's slice9 borders land in. Their
  regions may then stay unpacked. A button's plate follows the engine button's
  state tint without its easing, and the engine still moves and scales it.
  Engine widgets that take only atlas regions -- `nt_ui_slider`,
  `nt_ui_progress`, the switches and the dropdown -- keep drawing the art,
  and their kit style accessors assert when it is not bound. The kit's rows
  do not need it: without the art the toggle, checkbox and radio are kit
  buttons holding shapes, the slider row keeps the engine slider for input and
  draws track, fill and thumb at `nt_ui_slider_thumb_pos`, and the dropdown
  trigger is the neutral button look over the engine's flat baseline. With the
  art packed, the rows keep it. With `art.white`
  set to the atlas's white pixel the scroll thumb draws from it, tinted: a
  flat bar rather than the pill. `ui_kit_disc_begin` needs a FIXED width. The shapes are flat: a sheet's gloss and rivets (Studio B)
  are not drawn, so that sheet keeps its art.
- `tools/gen_ui_kit.py --tokens <sheet> --out <assets/ui>` — draws the slice9 art
  from a token sheet. `art.gloss` (0 when omitted) lightens the top rim of the
  fixed-colour surfaces — panel and tile — so they read as moulded plastic
  rather than flat fills. Grayscale art cannot take it: a runtime multiply tint
  has no headroom above white.

The `on_world` role outlines its glyphs, which the engine builds only with
`NT_FONT_EMBOLDEN_ENABLED=ON`; a consumer that uses the role sets that option
before adding the engine.

Text takes `const char *`: the kit does not know how its consumer localizes. A
consumer with a localization wrapper keeps ONE place where its string type
becomes a raw pointer and calls the kit from there.

## Validation

- `test_ui_scale` — the canvas rule's invariants: an authored size keeps its
  share of the short edge at every window size and density, the scale is
  orientation-free and monotone, nothing floors it in a small window, a CSS-pixel
  safe area survives the round trip, and a window the platform has not sized yet
  still yields a finite canvas.
- `test_ui_shape_pack` — every packed shape field reads back exactly as the
  shader unpacks it, lengths clamp inside their 12-bit field, and a zero style
  is not the plain-sprite block. It also holds the rules the widgets derive:
  a pressed look differs from idle for every lip, disabled is the off fill,
  a body near the scroll limit does not flip its scissor frame to frame, and
  a choice row's padded touch zones never overlap.
- `test_ui_screen_pack` — however many labels crowd one spot, every one that
  shows lies inside the bounds and clear of the taken boxes and of each other,
  and a crowd spreads across instead of into one column.
- `node --test features/ui-kit/tests/tokens_parity.test.mjs` — the token sheet
  the art generator reads and the tokens compiled into `ui_tokens.c` are the
  same numbers. Without it a repaint lands in the art and not in the styles.
- `node features/ui-kit/example/native/tools/lab.mjs test` — the lab's theme
  sheets and its compiled tokens agree, and the pack builder and the theme
  table name the same theme folders. `lab.mjs build` and `lab.mjs shots` are
  the kit's own consumer proof: every widget in the real renderer.
- A consumer proves the rest with frames, not asserts: layout and readability
  are judged by looking (`ai_studio/runtime_automation/responsive_viewports.py`).

## Source antialiasing and alpha contract

The standard token sheet uses working supersample 16 and BOX area downsampling.
The default for an omitted `art.resample` is BOX; an explicit filter remains
supported. Area averaging avoids the faint ringing of the previous Lanczos
export around high-contrast UI silhouettes. It smooths both transparent edges
and opaque fill/rim boundaries without changing export sizes or slice9 geometry.

The consumer owns the matching renderer setup: premultiplied atlas, matching
sprite/text output, and `nt_blend_alpha_premultiplied()`. Mipmapped UI atlases
use trilinear minification and linear magnification. See INSTALL for wiring.
The template carries these defaults into newly created games; existing copies
need their own material and asset update. Source art from other libraries must
not be replaced with generated art solely because filenames match.

## Compatibility

Contract version in `feature.json`. Version 5.0 breaks the looks for a game
that draws its windows and rows in its own look: `ui_kit_button_begin_styled`
takes a `ui_kit_button_look_t` (disabled plate and reach), the dialog look
gains band height, title and close placement, a rule and `calm_scroll`, its
square close is a `ui_kit_button_look_t`, and the dialog's scroll room no
longer subtracts the body's top padding twice. `ui_kit_slider_row` takes a
separate value text and a look, `ui_kit_choice_row` a look, and
`ui_kit_dropdown_row` an enabled flag, and `ui_kit_dropdown_row_styled` draws
it in a look; segments, list rows and the trigger carry string names. A settings row is at least a touch height tall
instead of exactly one. Version 4.1 adds looks
(`ui_kit_look_*`, `ui_kit_look_state`, `ui_kit_button_begin_styled`, the dialog
look with its scrolling body), `ui_kit_icon_rotated`, `ui_kit_choice_row` and
shape-drawn settings rows under a shape theme without switch or slider art,
where those rows asserted before; nothing existing changes. Version 4.0 folds `UI_LAYER_BG`,
`UI_LAYER_FILL` and `UI_LAYER_IMG` into one `UI_LAYER_SURFACE` and renumbers
`UI_LAYER_ICON` (now its own layer under the text shadow), `UI_LAYER_TEXT_SHADOW`
and `UI_LAYER_TEXT`. A surface nested in another kit surface now draws over it at
any depth; a consumer that relied on a later, lower-layer sibling drawing
under an earlier one orders them by declaration instead. Version 3.1 adds `ui_world_anchor.h`
and `ui_screen_pack.h`, two pure helpers for text over the world; nothing
existing changes. Version 1.7 adds the glyph set
(`ui_icons.h`, `assets/icons/`, the `icons` art slots and
`ui_theme_art_bind_icons`), draws the close cross and the dropdown chevron
from it when bound, and moves the round close to the neutral tile colour.
Version 1.6 adds the `header` colour token
and art slot (0 = shell), the titled plate and dropdown row, the tinted plate
and disc, the theme scroll style, the disabled-label role, outlined action
labels (a consumer builds the engine with `NT_FONT_EMBOLDEN_ENABLED=ON` or the
outline is not drawn), and moves every Studio B contour to the shell colour.
Version 1.5 adds the opt-in Studio B sheet
and `on_action` (zero keeps the historical `on_panel` action-label colour for
existing game-owned initializers), the composites, the `counter` and
`on_world` label roles, the toggle, checkbox and radio styles, and the UI
Lab; it also binds the engine progress bar to the design-size track and fill
(the engine bakes those borders at source size, so the 4x pair drew bloated).
Existing entry points and canvas behavior are unchanged. Exact
consumer dependency records still need a deliberate version acknowledgement
before strict package validation; this does not migrate their frame mode.

- **PATCH** — art the generator draws differently at identical tokens, a comment,
  an internal helper. Consumers rebuild and regenerate; no source change.
- **MINOR** — a new token field with a default, a new widget, a new style role,
  or an existing widget drawn differently at the same tokens because it was
  drawn wrong. Existing consumers keep compiling; a consumer with its own
  token sheet gets the new field's default until it adds it.
- **MAJOR** — a removed or renamed public symbol, a changed `ui_tokens_t` layout
  that an existing sheet cannot satisfy, or a change to the canvas rule that
  moves layout at the same tokens. Consumers edit source and re-shoot their
  layout evidence.

## Extension points

**Repainting.** The tokens are the seam. A game that wants its own face copies
`tokens/studio_b.json` or `tokens/studio_default.json`, edits it, and passes its own `ui_tokens_t` to
`ui_theme_init` — the feature is not touched and nothing is forked:

```c
/* game's src/ui/theme.c */
static const ui_tokens_t GAME_TOKENS = {
    .shell = 0xFF2B1E14U, .panel = 0xFF4A331FU, /* ...the rest of the sheet... */
};
ui_theme_init(&GAME_TOKENS, &art);
```

and the art is redrawn from the matching sheet:

```
node ai_studio/dev_environment/python_run.mjs features/ui-kit/tools/gen_ui_kit.py --tokens design/ui_tokens.json --out assets/ui
```

Partial overrides work the same way: start from a copy of the default struct,
change the fields that matter, pass that. A game that only wants different
action colours never touches geometry, and its art keeps regenerating from the
default sheet.

## Presets

`studio_default` remains the legacy template preset. `studio_b` is an opt-in
light game UI: dark blue-grey contours, shallow action ledges, white panels and
tiles, and white labels on its green action fill; blue is the rewarded-ad button
and nothing else, the way the portals read it. A new UI polygon/example selects
Studio B explicitly; an existing template or game stays on `studio_default`
until its game-owned theme binding chooses `ui_tokens_studio_b()` and regenerates
its matching art.

**Art.** `ui_theme_art_t` is by-value region refs, so a consumer may bind fewer
regions than the kit knows about; an unbound region simply does not draw.

**Widgets.** A game-specific widget composes the kit's pieces in game code
rather than growing this pack. What belongs here is what a second game would use
unchanged.

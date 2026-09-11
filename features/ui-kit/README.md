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
  the `UI_LAYER_*` order every surface sorts on. Composites: a labelled or icon
  button by role (`ui_kit_button`, `ui_kit_icon_button`), the round close
  (`ui_kit_close_button`, the thumb art in the danger colour), the tinted
  plate and disc containers (`ui_kit_plate_begin`, `ui_kit_disc_begin`: a
  socket, a swatch, a badge, where a Clay rectangle would draw an unantialiased
  border), a counter, a captioned meter, a badge, a titled plate
  (`ui_kit_dialog_begin`: the header band, the title on it, the close on the
  corner, the body), a sheet (that plate inside the engine modal), settings
  rows (toggle, checkbox, radio, slider, dropdown) that express the engine's
  stateful widgets in the kit's own art, the theme's scroll bar, and
  `ui_kit_color`.
- `example/native/` — the UI Lab: a standalone engine consumer with six scenes
  and four themes where every widget above is tried in the real renderer, and
  the example a game copies a screen from. Opt-in; nothing in it is compiled
  into a game or the template.
- `ui_safe_area.h` — the device's own insets, in CSS pixels.
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
- `node --test features/ui-kit/tests/tokens_parity.test.mjs` — the token sheet
  the art generator reads and the tokens compiled into `ui_tokens.c` are the
  same numbers. Without it a repaint lands in the art and not in the styles.
- `node features/ui-kit/example/native/tools/lab.mjs test` — the lab's theme
  sheets and its compiled tokens agree, and the pack builder and the theme
  table name the same theme folders. `lab.mjs build` and `lab.mjs shots` are
  the kit's own consumer proof: every widget in the real renderer.
- A consumer proves the rest with frames, not asserts: layout and readability
  are judged by looking (`devapi/responsive_viewports.py`).

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

Contract version in `feature.json`. Version 1.6 adds the `header` colour token
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
python features/ui-kit/tools/gen_ui_kit.py --tokens design/ui_tokens.json --out assets/ui
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

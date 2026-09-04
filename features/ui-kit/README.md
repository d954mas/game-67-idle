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

Two decisions carry the pack:

**Size.** The canvas is scaled off the SHORT edge with a density floor and a cap
(`ui_scale_policy.h`), not by fitting a landscape reference rectangle. The
engine's `NT_UI_SCALE_EXPAND` answers "how much of 1280x720 fits", which on a
360x640 phone means one UI unit is 0.28 device pixels and every widget arrives at
a quarter of its intended size. The short edge is what a hand spans in either
orientation, so the rule is orientation-free and a device turned sideways is not
a different design.

**Units.** Type and touch targets are stated in CSS pixels, the only unit that is
the same PHYSICAL size on a phone and on a monitor. Plates are a share of the
canvas with a CSS-pixel clamp. Getting this backwards is the classic pair of
bugs: a dialog sized in raw CSS pixels overflows a phone, and type sized as a
share of the canvas is unreadable in a hand.

The default look is the studio's, taken from the reference game the lead uses to
set the bar: a prototype should not have to design a UI before it has a game.

## Public surface

- `ui_tokens.h` — `ui_tokens_t`: colour, type ramp, geometry, canvas rule.
  `ui_tokens_studio_default()` is the look a new prototype wears.
- `ui_scale_policy.h` — `ui_scale_fit()` and `ui_scale_css_unit()`. Pure
  arithmetic, no engine types, so the readability rule is testable without a
  window.
- `ui_metrics.h` — `ui_frame_begin()` (the consumer's UI runtime opens the
  frame), then `ui_metrics()`, `ui_css()`, `ui_css_unit()`.
- `ui_theme.h` — `ui_theme_init(tokens, art)` and the mutable `g_ui_theme`
  carrying every engine style. The CONSUMER resolves the atlas regions, because
  it owns its pack builder and generated asset ids.
- `ui_kit.h` — the widgets: panel, tile, scrim, label, shadowed label, button,
  meter, slider style, touch-target height, and the `UI_LAYER_*` order every
  surface sorts on.
- `ui_safe_area.h` — the device's own insets, in CSS pixels.
- `tools/gen_ui_kit.py --tokens <sheet> --out <assets/ui>` — draws the slice9 art
  from a token sheet.

Text takes `const char *`: the kit does not know how its consumer localizes. A
consumer with a localization wrapper keeps ONE place where its string type
becomes a raw pointer and calls the kit from there.

## Validation

- `test_ui_scale` — the canvas rule's invariants: the short edge is capped, the
  scale is orientation-free and monotone in window size, one CSS pixel buys the
  same share of the canvas at any density, and a window the platform has not
  sized yet still yields a finite canvas.
- `node --test features/ui-kit/tests/tokens_parity.test.mjs` — the token sheet
  the art generator reads and the tokens compiled into `ui_tokens.c` are the
  same numbers. Without it a repaint lands in the art and not in the styles.
- A consumer proves the rest with frames, not asserts: layout and readability
  are judged by looking (`devapi/responsive_viewports.py`).

## Compatibility

Contract version in `feature.json`.

- **PATCH** — art the generator draws differently at identical tokens, a comment,
  an internal helper. Consumers rebuild and regenerate; no source change.
- **MINOR** — a new token field with a default, a new widget, a new style role.
  Existing consumers keep compiling; a consumer with its own token sheet gets
  the new field's default until it adds it.
- **MAJOR** — a removed or renamed public symbol, a changed `ui_tokens_t` layout
  that an existing sheet cannot satisfy, or a change to the canvas rule that
  moves layout at the same tokens. Consumers edit source and re-shoot their
  layout evidence.

## Extension points

**Repainting.** The tokens are the seam. A game that wants its own face copies
`tokens/studio_default.json`, edits it, and passes its own `ui_tokens_t` to
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

**Art.** `ui_theme_art_t` is by-value region refs, so a consumer may bind fewer
regions than the kit knows about; an unbound region simply does not draw.

**Widgets.** A game-specific widget composes the kit's pieces in game code
rather than growing this pack. What belongs here is what a second game would use
unchanged.

# Scenes and components

What each scene exercises, which kit and engine pieces it stands on, and what
a game takes from it. Ids are the `nt_ui_id` strings a DevAPI bot clicks.

## Scenes

| Id | Scene | Exercises | Kit pieces | Engine pieces | A game copies |
| --- | --- | --- | --- | --- | --- |
| `hud` | Игра | edge HUD over a picture: level in the `on_world` role, xp meter, wallet counter, options, two ability slots (selected, cooling), main action with a badge | `ui_kit_counter`, `ui_kit_meter_captioned`, `ui_kit_button`, `ui_kit_icon_button`, `ui_kit_badge`, tile, `ui_metrics` safe area and `portrait` | `nt_ui_image` (backdrop) | the edge recipe: top row, spacer, hint, bottom row |
| `upgrade` | Улучшения | a sheet with a scrolled list of cards; per card icon socket, name, current → next, price button in affordable / unaffordable / locked / maxed states | `ui_kit_sheet_*`, tile, `ui_kit_icon_button` | `nt_ui_scroll_*`, `nt_ui_state_clear` on leave | the card composition and the sheet; numbers and the purchase rule stay the game's |
| `result` | Результат | a sheet with no title row: centred title, two reward tiles, primary and secondary action; claiming changes the wallet and level | `ui_kit_sheet_*` (no close button), tile, buttons | | the reward composition |
| `settings` | Настройки | the studio's settings screen: scrimmed kit panel that fits its content, three volume rows, language picker, a small red Reset that opens a confirmation sheet (Cancel is the default, the red action lives only there), a round cross on the panel corner and a full-width Close at the bottom (the cross is what a player looks for, the bottom button is what a thumb reaches); one layout at every size, no scroll | `ui_kit_scrim`, `ui_kit_panel_*`, `ui_kit_close_button`, `ui_kit_slider_row`, dropdown style, `ui_kit_sheet_*` for the confirmation | `nt_ui_block_pointer`, `nt_ui_combo_*` | the whole screen; a game binds the rows to its settings state (a studio game's `settings_screen.c` is the localized twin) |
| `components` | Компоненты | the catalogue on a flat ground: button kinds, disabled and icon variants, the type ramp, counters, meters, the engine progress bar, sliders, toggles, checkbox, radios, dropdown, panel and tile, a dialog opened from inside a scrolled list | everything above, `ui_kit_toggle_row`, `ui_kit_checkbox_row`, `ui_kit_radio_row` | `nt_ui_progress` | single primitives, and the "sheet declared outside the scroll" rule |
| `themes` | Темы | theme rows with swatches; picking one re-initialises every style while the demo state stays; a sample card under the current theme | `ui_theme_init` seam | | the theme binding recipe (`lab_theme.c`) |

Overlay scenes draw the world first. `upgrade` and `result` keep the HUD under
their sheet, because the engine modal dims everything below its z-band;
`settings` draws only the world under its panel, because a scrim at zIndex 0
dims only the layers below the panel's own and HUD text would show through.

Two dialog recipes on purpose. A sheet (`ui_kit_sheet_begin`) is the engine
modal: backdrop, occluder, Esc and backdrop close, open/close tween. The
settings screen is the studio's existing recipe instead: `ui_kit_scrim` plus
`nt_ui_block_pointer` and a floating kit panel that fits its content. It stays
that way because an engine slider inside a scroll inside a modal trips the
walker (see findings), and because every shipped game already has it.

One layout at every size. The canvas is 390 units across its short edge on a
phone, a tablet and a monitor alike (`ref_short`, no floor and no cap), so a
column that fits 390 units fits everywhere and nothing branches on
orientation. The lab's scene list is hidden under the overlay scenes for the
same reason a game has no scene list: the dialog gets the whole canvas.

## What moved into the kit

Composites the lab proved across scenes now live in `features/ui-kit`
(`ui_kit.h`, version 1.5): `ui_kit_button`, `ui_kit_icon_button`, `ui_kit_icon`,
`ui_kit_counter`, `ui_kit_meter_captioned`, `ui_kit_badge`, `ui_kit_sheet_*`,
`ui_kit_toggle_row`, `ui_kit_checkbox_row`, `ui_kit_radio_row`,
`ui_kit_slider_row`, and the `counter` and `on_world` label roles. The lab
keeps only its own furniture (`lab_chrome.h`: swatch, section heading, flat
ground) and the theme table (`lab_theme.c`), which is a recipe, not a widget:
a game has one theme.

## Button roles

A screen picks a role (`ui_kit_button_kind_t`), never a colour, and every
theme keeps the same reading:

| Role | Colour | Used for | In the lab |
| --- | --- | --- | --- |
| `UI_KIT_BUTTON_NEUTRAL` | white tile, ink label | a regular action, Close, Cancel, an unselected ability | Опции, ЗАКРЫТЬ, Отмена |
| `UI_KIT_BUTTON_CONFIRM` | green (`go`) | yes, the main action, a purchase for coins with the price and the coin on the button | УЛУЧШИТЬ, the upgrade prices, ДАЛЬШЕ |
| `UI_KIT_BUTTON_AD` | blue (`info`) | a rewarded ad and nothing else: Poki reads a blue button as "watch an ad" | `hud/ad` (+240 with the play glyph), Реклама |
| `UI_KIT_BUTTON_DANGER` | red (`danger`) | a destructive action, always behind a confirmation | Сбросить прогресс, the round close cross |

`button_info` stays in the theme as an alias of `button_ad` for a game that
already binds it; `UI_KIT_BUTTON_INFO` is gone.

Stays opt-in and out of the template: the scenes, the demo state, the lab
navigation, the scripted pointer and capture, the four theme folders, the
backdrop.

## Findings

Kit, fixed in 1.5:

- A Clay rectangle with `cornerRadius` and `border` is drawn without
  antialiasing, so its corners show steps at every scale; the button plate and
  the thumb art do not. `ui_kit_plate_begin` and `ui_kit_disc_begin` are the tinted
  containers for a socket, a swatch, a badge.
- `progress` bound the 4x track and fill; the engine bakes progress borders at
  source size like the slider, so the bar drew bloated. Now the `_sm` pair.
- `amount` prints in the coin colour, which vanishes on a light tile; the
  `counter` role prints a number in ink.
- Text over the world had only the shadowed label; the `on_world` role is
  white ink inside the theme's contour, one label, no floating child.
- The kit ships no switch or checkbox art; the toggle, checkbox and radio
  styles use the small fill pill and the thumb, so no new region is needed.

Kit, open:

- `ui_kit_scrim` floats at zIndex 0, and the walker sorts one zIndex by UI
  layer, so plates and text a screen draws under the scrimmed panel on a
  higher layer show through the scrim and over the panel. Raising scrim and
  panel to zIndex 1 would fix it but trips the slider finding below. Until the
  engine orders roots by declaration inside a zIndex, a scrimmed panel covers
  a screen that draws nothing under it, and a sheet is used over a live HUD.
- The neutral button's hover is pure white on a near-white tile: invisible. A
  hover to `tile_dim`, or a scale-only hover, would read. Left as is because
  the template's look depends on it.
- Icons: the lab uses five CC0 demo glyphs; gear, close, check, lock and star
  are text or absent. A kit icon set is a separate asset task.

Engine, after the update to 37d7f117 (the lab builds and runs on it):

- Floating zIndex is relative to the enclosing floating (#433) and the walker
  paints by the baked band (#435): the corner close needs no z parameter any
  more, and the two z-order findings below are to be re-tested against this
  engine before they are filed.
- Glyph outlines (the `on_world` role) need `NT_FONT_EMBOLDEN_ENABLED=ON`;
  the lab's CMake sets it. A game using the role sets it too.
- `nt_atlas_opts_t.compress` is `nt_basisu_encode_opts_t` by value (`{0}` =
  raw); `nt_material_step` is gone. The template's `build_packs.c` and
  `main.c` still carry both and need the same two edits.
- `nt_ui_radial` declared as a floating child of a button drew a plain quad
  in the lab; the engine's showcase draws its radials in the main tree. The
  round close uses the thumb art; the radial material stays bound in
  `ui_theme_art_t.radial` for cooldown wedges, to be tried in-tree.

Engine (candidate issues, found on 623116f0, re-test on the current engine):

- An engine slider inside a scroll container inside any floating root with a
  zIndex above zero (an engine modal at its z-band, or a plain floating panel
  at zIndex 1) trips `nt_ui SCISSOR_START with both axes false requires
  non-empty bbox`: the thumb is a floating element with `clipTo =
  CLAY_CLIP_TO_ATTACHED_PARENT` at zIndex 0, Clay lays roots out in zIndex
  order, so its clip marker resolves before the parent root has a bounding
  box. Toggles and the dropdown inside a modal are fine. This is also why a
  scrimmed panel cannot simply be raised above a live HUD to dim it fully.
- At exit, `nt_resource_shutdown` unmounts the pack while the font still pins
  its blob and logs `unmount pack ... while blob pinned (pins=1)`. Fonts are a
  PIN_BLOB asset, and no public call unpins one; `nt_font_destroy` before the
  shutdown does not either. Harmless at exit; the engine's own examples take
  the same order.
- Keyboard focus for buttons, toggles and sliders is not in the public API
  (text input has it). The lab shows hover, pressed, selected and disabled;
  keyboard focus is not simulated.
- A region is keyed by the hash of its own name (`b/panel`); the atlas prefix
  in the generated macro comment (`ui/b/panel`) is a label. Hashing the
  prefixed string resolves nothing.

Lab:

- The scene list is compact chips (one row across a desk, two rows of three in
  a hand) and is not drawn under `upgrade`, `result` and `settings`; those
  return to the HUD through their own close, or Esc.

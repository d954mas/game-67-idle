# UI Lab (native)

A standalone Neotolis Engine consumer of the studio UI kit: six scenes, four
themes, the real renderer. It is where the kit's components and themes are
tried before a game takes them, and it is the example a game copies from,
one screen at a time. It is not a game and not a second UI runtime.

The visual direction is Studio B: white plates, dark blue-grey contours, a
green primary action, a short hard ledge under buttons, large game
typography. The other three themes are repaints of the same geometry.

Buttons are picked by role, never by colour, and the roles are the ones the
portals taught: neutral (white tile) for a regular action, close and cancel;
confirm (green) for yes, the main action and a purchase for coins, with the
price and the coin on the button; ad (blue) for a rewarded ad and nothing
else, because Poki reads a blue button as "watch an ad"; danger (red) for a
destructive action behind a confirmation.

## Build and run

```powershell
node features/ui-kit/example/native/tools/lab.mjs build
node features/ui-kit/example/native/tools/lab.mjs run
node features/ui-kit/example/native/tools/lab.mjs run --scene upgrade --theme night --size 390x844
node features/ui-kit/example/native/tools/lab.mjs configure --fresh   # drop CMakeCache.txt, then reconfigure
```

`build` configures `build/native-debug` with Ninja and clang (Debug), builds
the engine, the pack builder, the pack and the executable, and writes compiler
output to `build/native-debug/build.log`. The executable is
`build/native-debug/bin/ui_lab.exe` with `assets/ui_lab.ntpack` beside it.

Keys in the window: `1`–`6` open a scene, `T` cycles the theme, `Esc` closes a
sheet or returns to the HUD. The scene list at the top is lab chrome: it hides
under the overlay scenes so a dialog gets the whole canvas, as in a game.

The executable takes the same options as `run` plus a capture mode:

```text
--scene hud|upgrade|result|settings|components|themes
--theme b|forest|ember|night
--size WxH          window size in pixels
--frames N          quit after N drawn frames (or capture on frame N)
--shot file.ppm     write the framebuffer on the capture frame and quit
--tap X,Y           scripted press/release at framebuffer pixels; repeatable
--hover X,Y         scripted pointer position held from the first frame
--wheel N           N wheel notches at the hover point before the taps
```

A scripted pointer replaces the mouse for that run and goes through the same
viewport conversion, so a tap proves the real hit-test.

## Evidence

```powershell
node features/ui-kit/example/native/tools/lab.mjs shots          # six scenes x desktop/phone
node features/ui-kit/example/native/tools/lab.mjs shot upgrade-buy --scene upgrade --size 390x844 --tap 278,289
```

Frames land in `tmp/ui-lab/*.png`. The interaction matrix used for the first
acceptance: tap the HUD action (opens the sheet), buy an upgrade (wallet and
card change), close by the cross and by the backdrop (back to the HUD), tap a
volume slider, open the language list, flip the catalogue toggles, scroll the
catalogue with the wheel, pick a theme, and the HUD and sheets at 1280x800,
390x844, 320x568 and 844x390.

## Layout

```text
CMakeLists.txt        standalone project: engine + builder + kit + this example
src/main.c            engine boot, frame, keys, scripted pointer, capture
src/lab_ui_runtime.*  nt_ui context and the frame around it (kit opens the canvas)
src/lab_theme.*       theme table: tokens + atlas folder per theme, apply/cycle
src/lab.*             demo state, scene registry, navigation
src/lab_chrome.*      lab-only furniture: swatch, section heading, flat ground
                      (every player-facing widget is the kit's own)
src/scenes/*.c        one file per scene
src/build_packs.c     the pack: shaders, font, one UI atlas (4 themes x 9
                      regions, 5 icons, the backdrop)
themes/*.json         lab-owned token sheets (forest, ember, night); Studio B is
                      the kit's own tokens/studio_b.json
assets/ui/<theme>/    generated kit art per theme; not tracked, `tools/lab.mjs build` draws it
(icons)               the five CC0 Kenney icons come from templates/template/assets/icons
assets/packs/         provenance: license, origin, sha256 per file
tests/                node tests (theme sheet <-> C tokens parity)
```

`SCENES.md` maps every scene to the kit pieces it exercises and to what a game
copies from it.

## Take a component, take a scene, change the theme

**A component.** Every widget a scene shows is the kit's (`ui_kit.h`):
labelled and icon buttons by role, counter, captioned meter, badge, sheet,
settings rows. A game links the kit and calls them; nothing is copied. The lab
keeps only its own furniture in `lab_chrome.h`.

**A scene.** A scene file is a recipe: the HUD is edges over a picture, the
upgrade sheet is a scrolled list of tiles with one interactive part per row,
the result sheet is title, rewards, primary and secondary action, the settings
screen is the studio's own. Copy the file and replace `g_lab` with the game's
state. Keep the ids: `nt_ui_id("scene/element")` strings are what a DevAPI bot
clicks.

**A theme.** A theme is a token sheet plus a folder of generated art, because
panel and tile colours are baked into the PNGs. To add one:

1. Copy `themes/night.json` to `themes/<id>.json` and edit `colors`. Keep
   `type`, `geometry`, `canvas` and `art` as they are; a repaint changes
   colours, not layout.
2. Add a `ui_tokens_t` block to `src/lab_theme.c` with the same colours packed
   `0xAABBGGRR`, and a row to `THEMES[]`. `tests/lab_themes_parity.test.mjs`
   fails if the two disagree.
3. Add the id to `THEMES[]` in `src/build_packs.c` (the test checks the two
   lists match) and run `tools/lab.mjs art` to draw `assets/ui/<id>/`.
4. Rebuild. The theme appears in the Themes scene and under `--theme <id>`.

A game does the same with one theme: its sheet, its `ui_tokens_t`, its nine
regions bound through `ui_theme_init`. The feature is never edited.

## Boundaries

- Opt-in. Nothing here is compiled into the template or into a game. The
  template keeps `ui_tokens_studio_default()`; a game selects Studio B by
  calling `ui_tokens_studio_b()` in its own theme binding and regenerating its
  art from `tokens/studio_b.json`.
- Engine is read-only. Every call is a public engine API; the engine tree is
  not modified. What the lab found that belongs to the engine or the kit is in
  `SCENES.md` under findings.
- Assets. The kit art is generated from the token sheets at build and is not
  tracked. The icons are the template's CC0 Kenney files, read in place.
  The backdrop (`assets/scene/world.png`, manifest `assets/packs/ui-lab-backdrop`)
  is a downscaled copy of the bold browser example's preview-only generation,
  whose prompt and provenance sit in `../bold/world.provenance.json`; it is
  scene content for the lab, not a kit asset and not a shipping asset. The
  font is the engine's Lilita One face (OFL) packed with ASCII, the full
  Russian alphabet including `ё`, and the arrow, times, bullet and dash glyphs.
- No game. Wallet, upgrades, level and rewards are demo numbers that show what
  a card or a counter has to display. No music, no progression, no lore.
- Web build. Not wired in this slice; the CMake refuses an Emscripten
  configure rather than half-linking one. The pack has no native-only content,
  so the engine's ui_showcase web block is the recipe when it is wanted.

## Verify

```powershell
node --test features/ui-kit/tests/tokens_parity.test.mjs
node features/ui-kit/example/native/tools/lab.mjs test
node features/ui-kit/example/native/tools/lab.mjs build
node features/ui-kit/example/native/tools/lab.mjs shots
```

Then look at the frames. A canvas rule and a theme are judged by looking.

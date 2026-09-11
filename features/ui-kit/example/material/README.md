# Material-inspired UI kit preview

Status: interactive visual prototype for review, not the C runtime theme or a shipping game UI. The user requested our own neutral kit with game-owned visual styles.

## Inventory before edits

- Surface: reusable UI kit; game HUD, settings dialog, result panel and component states. No active game is being implemented.
- Existing analogs: ui_tokens.h, ui_theme.h, ui_kit.h and the template settings screen. The current kit already separates tokens, atlas region bindings and widget calls.
- Reuse decision: keep the existing C module and consumer seams; put an independent browser preview under its example directory. It does not create a second runtime UI library or modify existing consumers.
- Existing primitives: panel, tile, scrim, labels, buttons, meter and slider styles; the engine owns combo behavior.
- Prototype seams: semantic colors, typography, shape, spacing, elevation and motion; a single component tree consumes different game themes.
- Runtime gaps to assess after visual selection: named light/dark presets, focus styling, theme-controlled interaction effects and a documented font binding seam.

## Run

From the Studio root:

```powershell
node features/ui-kit/example/material/serve.mjs
```

Open http://127.0.0.1:4179. The server listens only on loopback and serves only this example's HTML/CSS/JS/JSON. It needs Node.js and no npm install. UI_KIT_PREVIEW_PORT selects another port.

## Files and ownership

- kit.css: shared component appearance, semantic roles and interactive states.
- themes.mjs: three example game themes, each with light and dark palettes.
- preview.css: workbench and example game composition; it is not part of a shipping kit.
- preview.mjs: theme editor and example interactions.
- index.html: shared screen templates and component samples.
- serve.mjs: local static preview server.

The same component DOM stays mounted when the theme changes. A game may replace theme values and compose its own screens. Typography uses a game-selected browser font stack. No game state, currency catalog, progression or music pack is coupled to the component styles.

The JSON export is explicitly neotolis.ui-preview-theme.v1. It records the selected prototype palette, typography, shape, density and elevation. It is not accepted by the C token generator and does not claim compatibility with ui_tokens_t.

Settings toggles, volume and language selection retain their values between dialog openings for the page session. The language select demonstrates a control; it does not translate the sample. The number board is static example content. Round completion, next level and replay demonstrate UI flow rather than implement a puzzle game.

## Review and proof

Verified in Chromium: six theme/mode combinations; text contrast in each palette; custom font, shape, density and depth; extreme white/black/yellow/gray accents with readable focus and indicator roles; JSON export; settings synchronization; modal focus cycle, Escape and return; inline close/reopen; disabled controls, segment/chip selection and text input. Layout checked at 1440×1080, 1280×800, 390×844 and 844×390, including mobile-to-desktop resize. Reduced-motion emulation disables transitions. No page errors or failed app-resource responses.

Screenshots and the temporary verification harness are under tmp/ui-kit-material-qa/. This is visual evidence for the browser prototype, not C/WASM rendering evidence. Production runtime files, default token sheet and existing game consumers remain unchanged.

The preview must demonstrate the same HUD, settings and result in three themes, custom color/shape/font changes, light/dark modes, keyboard focus, disabled buttons, toggles, slider input and dialog close/reopen. Check desktop, narrow phone and short landscape layouts, reduced motion, Escape and modal focus return. Visual screenshots judge layout; no tests pin taste values.

The board, score and reward are example game content. They do not add progression, inventory or currency dependencies to the shared kit. Browser controls use real text and deterministic CSS; the example contains no generated raster art.

## References

- https://m3.material.io/ — component, color, shape and state guidance.
- https://blog.google/products-and-platforms/platforms/android/material-3-expressive-android-wearos-launch/ — expressive typography and interaction reference.

This is an original Material-inspired visual design, not a claim of full Material component compliance. The prototype uses browser/system fonts; production games retain their own font assets and engine text renderer.

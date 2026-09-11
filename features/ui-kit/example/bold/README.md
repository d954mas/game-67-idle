# Bold game UI preview

The user selected original direction B: strong ink contours, compact game controls, short solid button depth, readable icons and prominent action states. The selected fake shot is `tmp/ui-directions-2026-09-11/original-game-kit/b-bold-play.png`.

This supersedes the Material example as the current visual direction. The earlier example remains a record of theme/interaction exploration. Neither browser example is a second production UI runtime.

## Pre-edit inventory

- Surface: studio UI kit preview, with an overlay HUD, ability controls, upgrade modal, settings and results. No specific game is being implemented.
- Analogs: existing `features/ui-kit` C primitives and token/atlas seams; two studio games with world-first overlay compositions; the prior Material browser example's theme and focus behavior.
- Reuse: native browser buttons, range inputs and modal dialogs for this review artifact. The production module remains the existing C kit using engine public UI and font APIs.
- Ownership: `kit.css` contains preview component rules; `themes.mjs` contains theme choices; `preview.css`, `index.html` and `preview.mjs` own demo composition and state. Currency, upgrades and rewards are sample content, not starter-template dependencies.
- Proof: run and inspect desktop, phone and short landscape layouts; keyboard modal return/trapping; settings retention; purchase availability and selected ability state; theme changes retaining state. Native/C/WASM proof remains required before any engine integration.

## Run

```powershell
node features/ui-kit/example/bold/serve.mjs
```

Open http://127.0.0.1:4180. No dependency installation is required. The static server is loopback-only and serves this example directory. `UI_KIT_BOLD_PORT` overrides the port.

## Verification

Chromium passes: theme changes preserve component identity and selected ability; purchases deduct the displayed price and update availability; settings retain values across close/reopen; modal Tab/Shift+Tab containment, Escape and opener focus return; result transition; JSON export; reduced motion; desktop, phone and short landscape without horizontal overflow. Text contrast was checked for all three palettes.

Two review findings were reproduced and fixed: a status/wallet overlap at 320px and focus loss when the final affordable purchase disables its button. The targeted browser checks now pass. Evidence and review screenshots live under `tmp/ui-kit-bold-qa/`, with `verification.json` and `showcase.png`.

## Boundaries

UI labels are actual browser text, shapes are deterministic CSS, and the simple original SVG pictograms are direction mockups only. The original generated world background is scene content; it is not the reusable kit's art. No vendor images, sprites, fonts, PSDs, prefabs or layouts are imported. Background generation prompt and provenance are recorded in `world.provenance.json`.

Theme export uses `neotolis.ui-preview-theme.v2`; it is a review format, not accepted by the C token generator. Production art, font selection, atlas preparation and engine interaction must follow the existing asset and UI pipelines after this prototype is reviewed.

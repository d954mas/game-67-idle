# Game UI Layout Reference

Read the sections needed for layout structure, retained UI state, clipping,
overflow, or responsive proof.

## Source Map

- Engine contract: `external/neotolis-engine/docs/neotolis_engine_spec_1.md`.
- Widgets: `external/neotolis-engine/engine/ui/nt_ui_*.{h,c}` and
  `external/neotolis-engine/examples/ui_showcase/`.
- Current game: `games/<game-id>/src/ui/`, `tests/`, `devapi/`, and
  `design/ui_ux/`; start runtime glue at `src/ui/ui_runtime.*`.
- Proof: `ai_studio/runtime_automation/README.md`, `pixel_health.py`,
  `ui_readability.py`, and `responsive_viewports.py` (QCLR_002 matrix,
  `--portal` reserved zones).
- Also load `nt-runtime-automation` for DevAPI/screenshots/`ui.tree`, and
  `nt-quality-checks` when selecting quality gates.

## Capability Inventory

- Layout: `CLAY_SIZING_*`, direction, gaps, alignment, padding, fixed/grow/fit,
  `nt_ui_scale`, `STRETCH`, `LETTERBOX`, `CROP`, `EXPAND`.
- Floating/clipping: `CLAY_ATTACH_TO_PARENT`,
  `CLAY_CLIP_TO_ATTACHED_PARENT`, `.clip`, `clipTo`, scroll clipping.
- Overlays: `nt_ui_modal`, `nt_ui_popup`, dropdown, tooltip, context menu, and
  game wrappers such as `game_modal`.
- Lists: `nt_ui_scroll_begin/end`, `nt_ui_vlist_begin/end`, scrollbar and
  scenario-specific scroll state.
- Retained state: `nt_ui_state`, clear APIs, used slots, close ownership, and
  whether capacity or lifetime is the real problem.
- Widgets: button, panel, image, label, checkbox/radio/toggle,
  slider/progress, input text, tabbar, rich text.
- Input: stable IDs, `ui.tree`, `ui.click`, `nt_ui_events`, pointer capture,
  modal gating; distinguish 2D UI from raycast-driven 3D UI.

Floating content can clip to its attached parent. Before adding a manual
scissor, inspect `external/neotolis-engine/engine/ui/nt_ui_input.c`: clipped
non-floating content plus floating text/caret/selection using
`CLAY_CLIP_TO_ATTACHED_PARENT`.

## Implementation Contract

- Reuse engine public APIs and current game wrappers before custom rendering.
- Keep layout Y-up; convert platform/capture coordinates only at boundaries.
- Use packed real fonts through the engine text stack.
- Treat `nt_ui_state` as owned view/interaction state. Clear owned IDs on
  close, or all state on a full transition, unless survival is intentional.
  Raise slot/probe limits only after an ID/lifetime audit.
- Prefer semantic IDs and state assertions over coordinate-only bots.
- Scale one window composition across PC and phone orientations by default.
  Backpack, chest, and shop windows may rearrange when the content needs the
  extra width; record the reason for each exception. Settings stay in one
  composition and fit without scrolling.

## Window Composition Check

The game owns `design/ui_window_contracts.json`: one entry per window with a
stable semantic root ID (`/window` for new windows), source draw function and scene or DevAPI open
commands. `single` layout and no scroll are the defaults. An adaptive layout
or named scroll region needs a content reason. Settings cannot opt out of
single-layout/no-scroll. See the
[contract schema and proof procedure](../../../../ai_studio/runtime_automation/window_layout_contract.md).

For local edits, use game-local DevAPI scenarios to exercise and capture the
affected windows and related modal scenes. Shared layout changes need coverage
of affected consumers. Keep the contract's complete catalog regardless of the
current check scope.

For full game acceptance, after a native DevAPI build run:

```powershell
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/window_layout_capture.py --game <game-id>
```

This command checks the whole game; it has no per-window filter. It opens each
declared window in portrait and landscape, checks modal
scene coverage, composition, clipping and scroll behavior, and saves
screenshots. It cannot judge readability or touch-target quality, so inspect
both captures and exercise the controls.

## Targeted Verification

- For changed layout math, UI state, or helpers, run the relevant native test.
- For retained state, loop open/close/reopen and check the intended slot baseline.
- Capture affected layouts on desktop and phone with a game-local responsive
  scenario or `quality_responsive`; inspect images and exercise the controls.
- Use `ui.tree` for geometry/hit IDs, `pixel_health.py` for blank/flat captures,
  and `ui_readability.py` when text readability needs checking.
- Report evidence paths and gaps; use an existing task log when applicable.

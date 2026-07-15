# Game UI Layout Reference

Load this after the short inventory in `../SKILL.md` when the change affects
layout structure, retained UI state, clipping, overflow, or responsive proof.

## Source Map

- Engine contract: `external/neotolis-engine/docs/neotolis_engine_spec_1.md`.
- Widgets: `external/neotolis-engine/engine/ui/nt_ui_*.{h,c}` and
  `external/neotolis-engine/examples/ui_showcase/`.
- Current game: `games/<game-id>/src/ui/`, `tests/`, `devapi/`, and
  `design/ui_ux/`; start runtime glue at `src/ui/ui_runtime.*`.
- Proof: `ai_studio/runtime_automation/README.md`, `pixel_health.py`,
  `ui_readability.py`, and game-local `devapi/responsive_viewports.py`.
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
- Use one adaptive PC/phone component contract unless design requires two.

## Verification

- Run the narrow native/unit test for layout math, UI state, or helpers.
- Loop open/close/reopen for retained state and verify used slots return to the
  intended baseline.
- Capture desktop and phone through `quality_responsive` or the game-local
  responsive scenario.
- Inspect `ui.tree` bounds and stable hit IDs.
- Use `pixel_health.py` for blank/flat captures and `ui_readability.py` when
  text readability matters.
- Record evidence paths in the task log.

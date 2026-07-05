---
name: nt-game-ui-layout
description: "Use for layout-affecting in-game UI work in this repository: new or changed HUDs, game screens, modal/dialogue sheets, menus, bottom navigation, responsive PC/phone layout, floating layers, clipping/scrolling/overflow, hit targets, or visual proof for a changed game UI surface. Forces a short widget/mode inventory before non-trivial edits so agents reuse existing Neotolis UI APIs. Skip for pure copy tweaks, asset swaps, or runtime automation tasks that do not change layout."
---

# NT Game UI Layout

Use this skill as the pre-edit gate for product-facing game UI layout. The goal
is not to memorize the whole engine UI surface; the goal is to discover the
relevant widgets, modes, examples, and proof tools before touching layout code.

## Required Gate

Fast path: for a pure copy, color, image, or obvious one-file tweak that does
not change layout structure, skip the full inventory. Name the adjacent wrapper
or primitive being reused and run proportionate verification.

For non-trivial layout work, produce this short inventory in working notes or
the task log before editing:

- Game/surface: explicit game id plus HUD, screen, modal/sheet, overlay, menu,
  list, text, etc.
- Existing analogs: nearby `games/<game-id>/src/ui/` files, tests, DevAPI
  scenarios, task logs, or accepted screenshots/handoffs.
- Primitives found: engine widgets, modes, wrappers, or examples that fit.
- Decision: reuse path, or why existing primitives do not fit.
- Proof: smallest native plus desktop/phone visual/runtime evidence.

Do not add a new reusable UI wrapper, custom renderer, manual scissor, or layout
primitive until this inventory shows the existing engine/game primitives do not
fit. Raw Clay composition inside an existing surface is fine when it follows the
engine contract and current game wrappers.

## Source Map

Start with the smallest matching source set:

- Engine UI contract: `external/neotolis-engine/docs/neotolis_engine_spec_1.md`.
- Engine widget APIs and examples: `external/neotolis-engine/engine/ui/nt_ui_*.h`,
  `external/neotolis-engine/engine/ui/nt_ui_*.c`,
  `external/neotolis-engine/examples/ui_showcase/`, and
  `external/neotolis-engine/examples/ui_3d_demo/`.
- Current game UI: `games/<game-id>/src/ui/`, `games/<game-id>/tests/`,
  `games/<game-id>/devapi/`, and `games/<game-id>/design/ui_ux/`.
- Current game runtime glue: `games/<game-id>/src/ui/ui_runtime.*`, then
  surface-specific files when present. For `rb-dark-rpg`, useful examples
  include `dialogue_panel`, `bottom_nav`, `equipment_screen`,
  `world_map_screen`, `combat_flow`, and tutorial callouts.
- Runtime proof tools: `ai_studio/runtime_automation/README.md`,
  `ai_studio/runtime_automation/pixel_health.py`,
  `ai_studio/runtime_automation/ui_readability.py`, and the game-local
  `devapi/responsive_viewports.py`.
- If proof requires DevAPI, screenshots, or `ui.tree` interaction, also load
  `nt-runtime-automation`. If selecting AI Studio quality gates, also load
  `nt-quality-checks`.

When updating this skill or auditing why it exists, read the historical
`references/friction-review-2026-07-05.md`. Do not load that review during
ordinary layout work unless the same failure pattern is recurring.

## Inventory Checklist

Search for the requested capability before coding:

- layout and sizing: `CLAY_SIZING_*`, `CLAY_LEFT_TO_RIGHT`,
  `CLAY_TOP_TO_BOTTOM`, `childGap`, `childAlignment`, padding, fixed/grow/fit,
  `nt_ui_scale`, `STRETCH`, `LETTERBOX`, `CROP`, `EXPAND`;
- floating and clipping: `CLAY_ATTACH_TO_PARENT`,
  `CLAY_CLIP_TO_ATTACHED_PARENT`, `.clip`, `clipTo`, scroll clipping;
- transient overlays: `nt_ui_modal`, `nt_ui_popup`, dropdown, tooltip, context
  menu, and game-local wrappers such as `game_modal`;
- lists and overflow: `nt_ui_scroll_begin/end`, `nt_ui_vlist_begin/end`,
  scrollbar visibility, scenario-specific scroll state;
- retained UI state: `nt_ui_state`, `nt_ui_state_clear`,
  `nt_ui_state_clear_all`, `nt_ui_state_used_slots`, screen/overlay close
  ownership, and whether `state_slots` / `state_probe_max` are capacity or
  lifetime problems;
- widgets: `nt_ui_button`, `nt_ui_panel`, `nt_ui_image`, `nt_ui_label`,
  checkbox/radio/toggle, slider/progress, input text, tabbar, rich text;
- text: real engine font/text renderer only; rich text/markup when emphasis or
  inline styling is needed; no handmade `draw_text` for product UI;
- input and hit targets: stable string ids, `ui.tree`, `ui.click`,
  `nt_ui_events`, pointer capture, modal/input gating;
- coordinate mode: default 2D UI vs `use_raycast_input=true` 3D UI, and the
  DevAPI read/write contract in Y-up layout pixels.

Known pitfall: floating content can clip to its attached parent. Before adding a
manual scissor or a bespoke workaround, inspect the existing pattern in
`external/neotolis-engine/engine/ui/nt_ui_input.c`: a non-floating clipped
content child, then floating text/caret/selection with
`CLAY_CLIP_TO_ATTACHED_PARENT`. For game-local examples, inspect
`games/<game-id>/src/ui/world_map_screen.c` when present.

## Implementation Rules

- Reuse `external/neotolis-engine` public APIs and current game wrappers before
  custom render/layout code.
- Keep internal game/world/UI layout logic Y-up. Convert Y-down platform or
  capture data only at documented boundaries.
- Keep user-visible text on the engine text stack with packed real fonts.
- Treat `nt_ui_state` cells as owned retained view/interaction state, not game
  state. For screens, sheets, modals, popups, lists, or overlays whose
  scroll/focus/hover/open/caret state does not intentionally survive close and
  reopen, clear the owned ids with `nt_ui_state_clear` on close, or use
  `nt_ui_state_clear_all` for a full screen transition. Raise `state_slots` or
  `state_probe_max` only after an id/lifetime audit proves the state is
  intentionally retained.
- Prefer semantic UI ids and game-state assertions over coordinate-only bots.
- For PC and phone, use one adaptive component contract unless the design
  explicitly requires separate systems.
- If a first attempt shows overlap, clipped text, or missed taps, inspect
  `ui.tree` bounds and matching engine examples before changing geometry.

## Verification

Pick the smallest proof that covers the changed surface:

- Native/unit tests for layout math, UI state, dialogue/equipment/combat logic,
  or game-local viewport helpers.
- For surfaces that create retained engine UI state, loop open/close/reopen and
  verify `nt_ui_state_used_slots` returns to the baseline, or to a documented
  intentionally retained baseline, before treating overflow as a capacity issue.
- `quality_responsive` or `games/<game-id>/devapi/responsive_viewports.py` for
  desktop and phone evidence. Use scenario hooks to open the exact screen.
- `ui.tree` bounds and stable ids for hit targets; do not rely only on labels or
  array indexes.
- `pixel_health.py` for blank/flat screenshots and `ui_readability.py` for text
  zoom checks when readability matters.
- Record evidence paths in the task log before marking work done.

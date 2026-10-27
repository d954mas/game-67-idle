---
name: nt-game-ui-layout
description: "Build or fix game HUDs, screens, menus, and responsive layouts using existing engine widgets. Skip copy-only and asset-only changes."
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

## Reference

After the inventory, load
[`references/layout-contract.md`](references/layout-contract.md) for the source
map, widget/mode checklist, retained-state rules, clipping pitfall, and proof
commands. Load only the sections the current surface needs.

## Invariants

- Reuse engine public APIs and current game wrappers before custom primitives.
- Keep layout Y-up and user-visible text on the engine font/text stack.
- Clear owned transient `nt_ui_state` on close; raise capacity only after an
  ID/lifetime audit.
- Prefer semantic UI IDs and one adaptive PC/phone contract.
- All HUD, windows and touch controls respect `ui_metrics()` safe insets.
  Web shells supply live CSS `env(safe-area-inset-*)` values through the UI Kit
  bridge; see `features/ui-kit/INSTALL.md#web-safe-area`. Do not bypass the
  common conversion with per-screen scaling, guessed device margins or a
  second safe-area inset. Verify nonzero top, side and bottom insets as well
  as zero; emulation does not prove physical-device browser reporting.
- Scale one window composition across desktop, portrait, and landscape by
  default. Change the arrangement only when the content genuinely needs it
  (for example, backpack, chest, or shop); record that exception. Settings
  keep one composition and fit without scrolling.
- Inspect `ui.tree` and matching examples before tuning broken geometry.
- Keep HUD out of the publish target's reserved zones (the Poki Pill, top-left);
  prove it with `ai_studio/runtime_automation/responsive_viewports.py --game <id> --portal <target>`.

## Verification

Pick the smallest native test plus desktop/phone runtime or visual evidence that
covers the changed surface. For retained state, loop open/close/reopen and
confirm used slots return to the intended baseline. Record evidence paths before
marking work done.

For every active game's window, maintain `design/ui_window_contracts.json` and
run the Studio-wide contract inventory (`window_layout_contract.py --studio-root .`)
to expose missing active-game declarations; frozen games are skipped. Then
run `ai_studio/runtime_automation/window_layout_capture.py --game <id>` after
a native DevAPI build. The shared checker must cover every declared window and
modal scene; review its screenshots. See
[`references/layout-contract.md`](references/layout-contract.md#window-composition-check).

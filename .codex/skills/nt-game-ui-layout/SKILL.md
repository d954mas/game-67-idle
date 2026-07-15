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
- Inspect `ui.tree` and matching examples before tuning broken geometry.

## Verification

Pick the smallest native test plus desktop/phone runtime or visual evidence that
covers the changed surface. For retained state, loop open/close/reopen and
confirm used slots return to the intended baseline. Record evidence paths before
marking work done.

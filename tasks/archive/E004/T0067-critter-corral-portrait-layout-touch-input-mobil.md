---
id: T0067
title: Critter Corral portrait layout, touch input, mobile web build
status: done
epic: E004
priority: P1
tags: []
created: 2026-06-16
updated: 2026-06-16
---

## What

Make Critter Corral work in PORTRAIT and with TOUCH, and stand up a mobile
WEB (wasm) build — without breaking the existing landscape desktop build.

## Done when

- [x] Layout is aspect-adaptive: portrait (h>w) lays pens on top/bottom edges
      with gates facing center; HUD, upgrade cards, FTUE, and title reflow and
      fit a 9:16 screen with no overflow/overlap.
- [x] Touch works: lure follows a held/dragged finger (primary active pointer);
      tap-to-start, tap-a-card, tap-restart register for touch (not mouse-only).
- [x] Verified natively at 540x960: corral_portrait.png (title) +
      corral_portrait_upgrade.png (cards fit) pass pixel-health audit.
- [x] Landscape (960x540) still works (title + upgrade captured, unchanged).
- [x] wasm build: game_seed -> index.html/js/wasm; pack synced into the web
      output dir at the fetch-relative path; loads + renders in a browser.

## Open questions

- DevAPI framebuffer capture is native-only, so there is no automated web
  screenshot via DevAPI; verified the web build with headless Chrome instead.

## Log

- Part A (portrait + touch): edited src/clean_seed_main.c —
  - corral_layout_pens: portrait branch (top/bottom edges, wide/short pens).
  - upgrade_card_rect + card draw/text: vertical stack in portrait, icon-left.
  - title badge row, lure orb, title/subtitle text: scale to width (fit helper).
  - Input: unified pointer tap helpers (primary_pointer / pointer_tap_pressed)
    so touch taps register; handle_input() now compiled on web too; restart
    marker is finger-sized and aspect-aware; lure tracks first active pointer.
  - FTUE tip only shows while PLAYING; beat-0 copy is device-neutral ("Drag").
- Part B (web): emsdk present (C:/develop/emsdk, emcc 4.0.19). Added an
  EMSCRIPTEN asset-sync in CMakeLists.txt copying critter_corral.ntpack into
  the web output under assets/runtime/critter-corral/ (nt_http fetches it
  relative to index.html). Guarded corral_behavior_loose_count under
  NT_DEVAPI_ENABLED (unused in the web/release -Werror build).
- Verified: native 540x960 + 960x540 captures pass audit; headless Chrome on a
  local static server boots the wasm, fetches the 2.1MB pack (HTTP 200), and
  renders the portrait title screen.
- Captures: build/captures/corral_portrait.png, corral_portrait_play.png,
  corral_portrait_upgrade.png, corral_landscape_title.png,
  corral_landscape_upgrade.png, corral_web_portrait.png.
- Helper: tools/critter_corral/capture_portrait.py (portrait proof shots).

- 2026-06-16: Increment 6 (portrait + touch + mobile web). Aspect-adaptive layout (portrait=h>w): pens top/bottom(+corners), upgrade cards vertical finger-tall stack, HUD/title/text fit the short edge. Touch fixed (real bug: nt_input_mouse_is_pressed only matched the mouse pointer slot -> finger taps never registered; now any active pointer taps + the lure follows a dragged finger; handle_input compiled on web too). WEB/WASM build works: emscripten present, wasm-release builds index.html/js/wasm; added CMake asset-sync to copy the pack into the web output (HTTP-fetched). Verified in headless Chrome 540x960 portrait: pack loads (7 assets), portrait title renders (corral_web_portrait.png). Native portrait verified (corral_portrait*.png, audit pass). Both builds clean -Werror. Phone: serve build/game_seed/wasm-release with a static server, open on phone in portrait.

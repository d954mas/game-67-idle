---
id: T0068
title: Critter Corral bespoke visual and UI UX pass
status: done
epic: E004
priority: P1
tags: [visual, ui, ux, assets, native]
created: 2026-06-16
updated: 2026-06-16
---

## What

Replace Critter Corral's placeholder-looking art/UI surface with a richer
casual/mobile visual pass while preserving the existing native gameplay loop.
Scope includes the pasture, critters, pens, lure/FX, HUD/modal UI assets, title,
play, upgrade, and portrait readability. Out of scope: new mechanics, balance
changes, web/mobile builds, audio, and engine/submodule edits.

## Done when

- [x] Sprite/UI source PNGs are replaced or regenerated with a cohesive bright
      casual style and the same runtime ids/pack path still load.
- [x] Native pack and game build succeed after the visual pass.
- [x] Desktop and portrait native captures show title/play/upgrade surfaces with
      clear first action, readable HUD/FTUE, touch-friendly upgrade choices, and
      no obvious overlap or debug-looking placeholder art.
- [x] Visual/product gate evidence is recorded with screenshot paths and any
      remaining visual debt.
- [x] Taskboard validation passes.

## Open questions

- None. Lead direction: "полностью сделать визуал" and "ui и ux тоже нужно
  сделать"; proceed with a native visual/UI/UX pass.

## Log

- 2026-06-16: Started. Session contract: goal = bespoke casual/mobile art + UI/UX
  pass for current native Critter Corral; non-goal = no gameplay/economy/web/audio
  expansion; proof = rebuilt `critter_corral_packs`, native desktop/portrait
  screenshots, visual/product gate, taskboard validation; stop = if screenshots
  still read as placeholder/debug, freeze expansion and iterate visuals.
- 2026-06-16: product gate PASS (desktop); review: gamedesign/projects/critter-corral/reviews/T0068_visual_ui_desktop_gate.md; screenshot: build/captures/corral_visual_ui_landscape_play.png; next: continue to the next narrow slice
- 2026-06-16: Regenerated source sprites (`critter`, `pen`, `grass`, `lure`,
  `spark`, `pip`, `card`, flag and color critter variants) via
  `py -3.12 tools/critter_corral/generate_sprites.py`; rebuilt
  `critter_corral_packs` and `game_seed`.
- 2026-06-16: Native proof captures: `build/captures/corral_portrait.png`,
  `build/captures/corral_portrait_play.png`,
  `build/captures/corral_portrait_upgrade.png`, and
  `build/captures/corral_visual_ui_landscape_play.png`.
- 2026-06-16: product gate PASS (portrait); review:
  `gamedesign/projects/critter-corral/reviews/T0068_visual_ui_portrait_gate.md`;
  screenshot: `build/captures/corral_portrait_upgrade.png`.
- 2026-06-16: responsive layout audit PASS for portrait upgrade cards; report:
  `gamedesign/projects/critter-corral/reviews/T0068_portrait_upgrade_layout_audit.md`.
- 2026-06-16: Taskboard validation PASS:
  `node tools/taskboard/cli.mjs validate`.

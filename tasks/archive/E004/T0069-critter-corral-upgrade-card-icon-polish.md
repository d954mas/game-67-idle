---
id: T0069
title: Critter Corral upgrade card icon polish
status: done
epic: E004
priority: P1
tags: [visual, ui, ux, native]
created: 2026-06-16
updated: 2026-06-16
---

## What

Polish the upgrade choice cards so the UI reads as juicy casual game UI rather
than black utility pictograms. Add a color/glow badge treatment behind runtime
upgrade icons while preserving existing card hit targets, runtime text, and
gameplay behavior.

## Done when

- [x] Upgrade card icons have a colorful game-like badge/glow in portrait and
      landscape.
- [x] Native game build succeeds.
- [x] Portrait upgrade screenshot shows readable text, stable touch cards, and
      no overlap.
- [x] Product/layout evidence remains passing after the polish.
- [x] Taskboard validation passes.

## Open questions

- None. This is a narrow follow-up from the visual completion audit.

## Log

- 2026-06-16: Started after completion audit found upgrade card icons still
  looked too much like black utility pictograms compared with the requested
  juicy casual UI direction.
- 2026-06-16: product gate PASS (portrait); review: gamedesign/projects/critter-corral/reviews/T0069_upgrade_icon_polish_gate.md; screenshot: build/captures/corral_portrait_upgrade.png; next: continue to the next narrow slice
- 2026-06-16: Added colored glow/badge backing behind runtime upgrade icons and
  removed bracket key labels that overlapped the landscape card foot. Keyboard
  1/2/3 behavior is unchanged; dot index hints remain.
- 2026-06-16: Native build PASS: `cmake --build build/_cmake/native-debug --target game_seed`.
- 2026-06-16: Native proof captures PASS:
  `build/captures/corral_portrait_upgrade.png` and
  `build/captures/corral_landscape_upgrade_polish.png`.
- 2026-06-16: responsive layout audit PASS:
  `gamedesign/projects/critter-corral/reviews/T0069_portrait_upgrade_layout_audit.md`.
- 2026-06-16: Taskboard validation PASS:
  `node tools/taskboard/cli.mjs validate`.

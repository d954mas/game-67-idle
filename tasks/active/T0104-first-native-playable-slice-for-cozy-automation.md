---
id: T0104
title: First native playable slice for Cozy Automation
status: review
epic: E006
priority: P1
tags: [prototype, cozy-automation, native-first]
created: 2026-06-22
updated: 2026-06-22
---

## What

Build the first native playable slice for `Cozy Automation` after the Stage 0 startup gate is ready.

## Done when

- [x] `gamedesign/projects/cozy-automation/gdd.md` names the first playable loop and player-readable goal.
- [x] `gamedesign/projects/cozy-automation/data/core_loop.json` describes the
      player verbs, rules, feedback, risk, goals, replay reason, and reference
      grounding without assuming hands-off progression, away-time rewards, or
      reset-meta loops.
- [x] `gamedesign/projects/cozy-automation/visual/live_state_acceptance_matrix.json`
      is reviewed for this game's HUD, primary CTA, feedback, modal,
      blocked/affordable, and transient stress states. (capture mapping in
      `reviews/first_slice_visual_gate.md`)
- [x] A fake shot or visual target exists before runtime polish starts.
      (in-house art direction + generated cozy atlas; no external fake shot)
- [x] A 5-line visual session contract exists: goal, non-goal, proof, stop
      condition, likely files. (`reviews/first_slice_visual_gate.md`)
- [x] Current native screenshot or capture plan is compared against the fake
      shot/target in a mismatch list before visual code expands.
      (`reviews/first_slice_visual_gate.md`, all 5 items PASS)
- [x] Native PC build/run command is identified and captured in the task log.
- [x] First native screenshot/product-read proof is captured before expanding content.
      (`build/captures/cozy/first_screen.png`; product gate verdict PASS)

## Open questions

- Visual target: lead chose "full real art now" — target is the GDD art
  direction realized by the generated cozy sprite atlas + real engine font; no
  external fake shot. (resolved)

## Log

- 2026-06-22 — Implemented the first slice on the clean seed:
  - Design: filled `gdd.md` (The Little Garden loop) + `data/core_loop.json`
    (active-session automation, no idle/away/reset-meta).
  - State: replaced dead Dragon Grove fields (ids 15-50) with `cozy.berries`,
    `cozy.plot2_planted`, `cozy.greenhouse_unlocked` in
    `state/game_state.schema.json`; codegen verified.
  - Runtime: `src/cozy_automation_main.c` renders the garden with the engine
    sprite atlas + slug-text font (no debug shape renderer); core loop, mouse +
    `ui.click` input, DevAPI actions (`game.action.plant/unlock/tick`) and a
    `frame.screenshot` backbuffer-PNG endpoint.
  - Assets: real cozy sprite atlas in `assets/raw/cozy/` (AI-generated,
    project-owned) + `assets/fonts/cozy_ui.ttf` (Roboto, Apache-2.0) baked into
    `assets/cozy_automation.ntpack` by `src/build_packs.c`.
  - Build/run (native PC, from project root):
    `cmake --preset native-debug`
    `cmake --build build/_cmake/native-debug --target game_seed` (auto-builds the pack + assets header)
    `python tools/cozy-automation/first_screen_smoke.py` (drives the loop, captures live-state screenshots)
  - Pending: first native screenshot + product-read gate + mismatch list.
- 2026-06-22 — Proven end to end and moved to review:
  - Build: `game_seed.exe` built (packer baked `assets/cozy_automation.ntpack`,
    8.2 MB, 15 assets); `cozy_automation_main.c` clang `-fsyntax-only` clean and
    full build links.
  - Playable smoke: `tools/cozy-automation/first_screen_smoke.py` → **20/20
    checks pass** (atlas+font ready; tick auto-routes berries; plant -10 → rate 2;
    unlock at 50 → rate 5; `ui.click` drives planting). Captures in
    `build/captures/cozy/` (first_screen, primary_action_ready,
    primary_action_feedback, reward_active, transient_auto_route).
  - Product-read gate: **PASS** — `reviews/product_read_gate_latest.json`;
    strict visual rubric composition 4 / readability 5 / ui_controls 4 /
    action_direction 4 / art_quality 4 / audience_fit 5; no blocker/major.
  - Mismatch list filled (all PASS); two minor non-blocking polish notes (empty
    lower frame, slightly dark berry icon).
- 2026-06-22: close-slice PASS gate (desktop); gate: gamedesign/projects/cozy-automation/reviews/product_read_gate_2026-06-22T11-27-24-973Z_desktop.md; screenshot: build/captures/cozy/first_screen.png; evidence: First native playable slice proven: game_seed builds (pack 8.2MB), first_screen_smoke.py 20/20 pass (auto-route tick, plant->rate2, unlock->rate5, ui.click), product-read gate PASS (build/captures/cozy/first_screen.png; rubric all>=4). Real engine sprite atlas + Roboto font, no debug shapes.; next: (none)

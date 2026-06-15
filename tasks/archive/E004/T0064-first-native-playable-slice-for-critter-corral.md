---
id: T0064
title: First native playable slice for Critter Corral
status: done
epic: E004
priority: P1
tags: [prototype, critter-corral, native-first]
created: 2026-06-15
updated: 2026-06-15
---

## What

Build the first native playable slice for `Critter Corral` after the Stage 0 startup gate is ready.

## Done when

- [ ] `gamedesign/projects/critter-corral/gdd.md` names the first playable loop and player-readable goal.
- [ ] A fake shot or visual target exists before runtime polish starts.
- [ ] A 5-line visual session contract exists: goal, non-goal, proof, stop
      condition, likely files.
- [ ] Current native screenshot or capture plan is compared against the fake
      shot/target in a mismatch list before visual code expands.
- [ ] Native PC build/run command is identified and captured in the task log.
- [ ] First native screenshot/product-read proof is captured before expanding content.

## Open questions

- Which named references or fake shots define the visual target?

## Log

- 2026-06-15: Built the mechanical core moment in src/clean_seed_main.c (critter sim: wander+separation+lure attraction; matching-color capture with squash+particle burst+pen flash+chain boost; continuous waves; score). Added DevAPI game.state / game.reset_playtest / game.capture.framebuffer (glReadPixels->PPM, unblocks automated screenshots). Builds clean under -Werror; ran via devapi running_game, captured build/captures/corral_core.png, pixel_health audit PASS, score 0->5/6. NOTE: rendered with the DEBUG shape renderer only — lead clarified shape renderer is debug-only; visual layer is being moved to free-asset sprites next (Codex bespoke art later).

- 2026-06-15: First playable slice achieved. Sprite-rendered core moment (lure herds critters; matching color pops into its pen with squash+particles+flash+chain; continuous waves; fontless score/goal HUD). Visual gate PASS vs the concept DIRECTION (corral_review2.png; composition4/readability5/ui4/action4/art4/audience5; pixel audit stdev 24.4). Builds clean -Werror. Slice done; expansion tracked in T0065.

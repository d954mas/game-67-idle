---
id: T0003
title: "P1: Readable Frost Keep first screen with real sprites + visual gate"
status: idea
epic: E001
priority: P1
tags: [voxelheim, visual-first, render, gate]
created: 2026-06-16
updated: 2026-06-16
---

## What

Render the static "Frost Keep Approach" first screen in the native build with
real sprites from T0002's atlas — snowy path, Frost Keep + glowing portal, hero,
distant dragon, full HUD overlay — reaching the Theme-A fake-shot direction.
Depends on T0002. **Visual-first freeze: this gate comes before any systems.**

## Done when

- [ ] Native screen composes the scene with real atlas sprites (no shape
      renderer for game visuals).
- [ ] Native build/run/screenshot commands for a new screen confirmed + logged
      (`game-feature-iteration` discovery).
- [ ] Screenshot-vs-fake-shot mismatch list written.
- [ ] `node tools/ai.mjs gate --visual-strict` reaches the Theme-A direction
      (composition, readability, UI, art quality, audience fit).
- [ ] Native screenshot proof captured.

## Open questions

- New dedicated screen vs extend the `clean_seed_main` scaffold?

## Log

- 2026-06-16 created (depends on T0002).

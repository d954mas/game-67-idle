---
id: T0313
title: "rb-dark-rpg polish: стражник, подсветка и портрет в рамке"
status: review
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, polish, dialogue, art]
created: 2026-07-05
updated: 2026-07-05
---

## What
Repair first guard art/highlight reliability and dialogue portrait layering.

## Done when

- [x] Hub object sprites still render if mask-glow material is unavailable.
- [x] Guard portrait art is clipped to its badge, shifted right, and the frame is drawn on top.

## Open questions

## Log
- 2026-07-05: Start: inspect guard location art/highlight and dialogue portrait layering.
- 2026-07-05: Fixed dialogue portrait clipping/layer order and made hub object sprites independent from mask-glow readiness so guard art still renders when glow material is unavailable.
- 2026-07-05: Verification: native game target built; native test suite passed. DevAPI visual gate was attempted but blocked by TCP request timeout after listener startup.

---
id: T0312
title: "rb-dark-rpg polish: нижняя навигация и экран снаряжения"
status: review
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, polish, equipment, nav]
created: 2026-07-05
updated: 2026-07-05
---

## What
Fix hover/press feedback that breaks visual consistency: the "Беру снаряжение" button should not turn blue, and bottom nav labels should scale with the button art.

## Done when

- [x] Dialogue button hover/pressed tints use the engine packed color order and stay warm.
- [x] Bottom nav label font size follows the same visual scale as the nav button art.

## Open questions

## Log
- 2026-07-05: Start: equipment hover color and bottom nav pressed scale alignment.
- 2026-07-05: Fixed dialogue button hover tint packing and scaled bottom-nav label font with the same visual scale as the button art.
- 2026-07-05: Verification: native game target built; native test suite passed. Bottom navigation label scale now follows button visual scale.

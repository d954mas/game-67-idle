---
id: T0311
title: "rb-dark-rpg polish: карта, цель и новый путь"
status: review
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, polish, map]
created: 2026-07-05
updated: 2026-07-05
---

## What
Remove reward/goal presentation that does not fit the game style: no "new path" reward preview, and the dialogue goal marker should not be a bright yellow circle.

## Done when

- [x] The first guard completion preview shows the token and XP, not an unlock/path reward.
- [x] The dialogue "Цель" marker uses a dark framed badge instead of the yellow circle.

## Open questions

## Log
- 2026-07-05: Start: map target marker style and no new-path reward preview.
- 2026-07-05: Removed new-path reward preview via T0314 and replaced bright yellow objective circle with a dark framed task badge.
- 2026-07-05: Verification: native game target built; native test suite passed. DevAPI visual gate was attempted but blocked by TCP request timeout after listener startup.

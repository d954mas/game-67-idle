---
id: T0314
title: "rb-dark-rpg polish: награды, предметы, XP и золото"
status: review
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, polish, rewards]
created: 2026-07-05
updated: 2026-07-05
---

## What
Improve dialogue reward presentation for immediate/completion items and numeric XP/gold rewards.

## Done when

- [x] Immediate and completion reward cells are larger.
- [x] XP/gold amounts render inside a wider overlay that can fit values like `+999`.
- [x] "New path" unlock reward is not shown as a reward preview.

## Open questions

## Log
- 2026-07-05: Start: reward cells, completion item count, XP/gold width, hide new-path unlock reward.
- 2026-07-05: Removed unlock reward preview, enlarged reward cells, and moved numeric amounts into a wider in-cell overlay for +999-style values.
- 2026-07-05: Verification: native game target built; native test suite passed. Dialogue reward cells were widened and item rewards now use larger slots.

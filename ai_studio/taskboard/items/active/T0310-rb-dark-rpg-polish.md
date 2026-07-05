---
id: T0310
title: "rb-dark-rpg polish: стартовый текст и буква е без Ё"
status: review
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, polish, content]
created: 2026-07-05
updated: 2026-07-05
---

## What
Remove the odd opening wording about the Dragon leaving/returning and normalize player-facing authored game text so the font does not need Cyrillic Ё/ё glyphs.

## Done when

- [x] The first guard line no longer contains the extra Dragon-return/leave phrase.
- [x] Real authored `Ё/ё` occurrences in rb-dark-rpg data are replaced with `Е/е`.
- [x] A focused grep verifies there are no remaining authored `Ё/ё` data hits.

## Open questions

## Log
- 2026-07-05: Start: remove odd opening dragon-return line and normalize Ё/ё in player-facing game text.
- 2026-07-05: Changed opening guard line and normalized real Ё/ё occurrences in authored game data. Remaining rg hits are mojibake byte sequences in legacy C literals, not actual Cyrillic Ё.
- 2026-07-05: Verification: taskboard validation passed; native game target built; native test suite passed. Authored JSON text uses е instead of Ё; remaining grep hits are mojibake C literals outside authored copy.

---
id: T0403
title: Prove bright Moon Bloom runtime reveal and compressed size budget
status: done
project: P004
epic: E017
priority: P0
tags: [visual-proof, size, web]
created: 2026-07-11
updated: 2026-07-13
---

## What

After T0404 locks the doll/style and six-item Moon/Bloom/Flame mini-capsule,
build a narrow production-quality Dress/Pair/Reveal spike with polished
Moon+Bloom and Moon+Flame results. T0405-T0407 later generalize these proven
seams; they do not create competing implementations.

## Done when

- [ ] Task is blocked until T0404 accepts the master doll, style bible and mini-capsule.
- [ ] Dreamgarden and Eclipse proofs run in portrait and landscape and are strongly contrasting.
- [ ] Runtime frames are strong enough to serve as text-free thumbnail candidates.
- [ ] In 10 cold tests, >=8 connect clothing to magic and >=6 voluntarily begin a second contrasting outfit.
- [ ] `_full` runtime duplicates are removed and thumbnails reuse cropped layers.
- [ ] Atlas compression and alpha-edge quality are proven on the real doll and reveal.
- [ ] Projected full initial payload is <=6,500,000 bytes with documented byte budget.
- [ ] Build-enforced byte inventory sums every initial network file and fails above exactly 6,500,000 bytes.
- [ ] Budget includes NPC/crowd, personalization, audio, platform adapter and all initial files.
- [ ] Failure of either visual or size gate blocks T0408 mass content production.

## Open questions

- Exact compression mode/quality after alpha-edge comparison.

## Log

- Baseline 2026-07-11: release `game.ntpack` is 54,289,384 bytes and is a hard fail.
- 2026-07-13: Closure: waived; reason: prototype closed by lead before acceptance; evidence: lead decision 2026-07-13; committed pause 4697cd445 and .planning/.continue-here.md record incomplete gates
- 2026-07-13: Quality: not-applicable; reason: closure records cancellation and does not claim implementation or acceptance

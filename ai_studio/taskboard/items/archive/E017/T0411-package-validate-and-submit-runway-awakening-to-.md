---
id: T0411
title: Package validate and submit Runway Awakening to 500-player test
status: done
project: P004
epic: E017
priority: P0
tags: [release, poki, player-fit]
created: 2026-07-11
updated: 2026-07-13
---

## What

Produce the clean Poki artifact, complete runtime/visual/portal QA, conduct cold
playtests and submit the accepted build to a 500-player Player Fit test.

## Done when

- [ ] Fresh Poki release artifact is <=6,500,000 initial bytes and contains only the Poki adapter.
- [ ] Artifact has no DevAPI, debug UI, source maps, tests, disallowed external requests or untracked assets.
- [ ] Native/unit/DevAPI/save/responsive/browser/ad/lifecycle checks pass with recorded evidence.
- [ ] Poki Inspector file-size, loading, scaling, QR/mobile and Event Log checks are clean.
- [ ] In five recorded cold playtests, >=4 complete the first reveal without help, >=4 explain that main+accent caused it, and >=3 voluntarily start round two.
- [ ] 500-player results are evaluated against official Player Fit and internal funnel thresholds.
- [ ] Official gate is average >3 minutes and >=125/500 over 3 minutes; internal go/iterate/kill table exactly matches the accepted GDD.
- [ ] Critical failure denominator is gameplay starts; any reproducible softlock blocks submission even if aggregate remains below 1%.
- [ ] Accessibility passes 44px targets, contrast, non-color state, flash <=3 Hz, mute and reduced-effects gates.
- [ ] Quality evidence records QCLR_001/QCLR_002/QART_001/QASSET_001/QDES_001/QTECH_001 outcomes.
- [ ] Go, one focused iteration, or kill decision is logged with evidence; no vanity declaration of success.

## Open questions

- Submission date and portal category after the final thumbnail/title test.

## Log
- 2026-07-13: Closure: waived; reason: prototype closed by lead before acceptance; evidence: lead decision 2026-07-13; committed pause 4697cd445 and .planning/.continue-here.md record incomplete gates
- 2026-07-13: Quality: not-applicable; reason: closure records cancellation and does not claim implementation or acceptance

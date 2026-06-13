---
id: T0011
title: Research Cow Evolution loop and balance for 67 World
status: review
epic: ""
priority: P1
tags: [research, gdd, balance, reference]
created: 2026-06-12
updated: 2026-06-12
---

## What

Research Cow Evolution as the closest genre reference, then decompose its loop,
UI, progression, balance shape, and copy-risk into actionable `67 World`
guidance.

## Done when

- [x] Reference research doc exists under `gamedesign/meme-evolution/`.
- [x] Research uses source quality labels and links.
- [x] Cow Evolution deconstruction has a Reference Lock, source matrix,
      Definition of Ready checklist, observation ledger, current native capture
      mismatch, and a clear ready/not-ready scope.
- [x] Borrow/avoid/copy-risk notes are explicit.
- [x] The doc translates Cow Evolution patterns into `67 World` mechanics.
- [x] First-slice balance implications are recorded.
- [ ] User reviews whether the research direction is enough or wants deeper
      competitor analysis.

## Open questions

- Should `67 World` worlds be meme-theme worlds, such as Fruit World, Portal
  World, Arcade World?
- Should customization be a separate layer or each costume be a collectible
  67 variant?
- Should the first playable version support manual drag merge, one-click merge,
  or both?

## Log

- 2026-06-12: Added `gamedesign/meme-evolution/reference_research.md` with Cow
  Evolution loop, balance, UI, and copy-risk decomposition.
- 2026-06-12: Added corrective pass
  `gamedesign/meme-evolution/cow_evolution_deconstruction_v2.md` after user
  feedback that current gameplay/UI does not read like Cow Evolution. Checked
  public store pages, screenshots, a gameplay video page, art references, and a
  review with gameplay screenshots. Key correction: Cow Evolution is
  field-first, with crates and creatures in a living world; 67 World must
  redesign the native screen around field interaction before expanding content.
- 2026-06-13: Updated
  `gamedesign/meme-evolution/cow_evolution_deconstruction_v2.md` against the
  Reference Study Definition of Ready. The doc is now ready for field-first
  screen grammar, reward placement, and copy-risk translation, but explicitly
  not ready for exact first-minute timing, one-hour balance, retention, or
  monetization pacing without a separate deep video/player transcript.
- 2026-06-13: Evidence passed:
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- gamedesign/meme-evolution/cow_evolution_deconstruction_v2.md tasks/active/T0011-research-cow-evolution-loop-and-balance-for-67-w.md tasks/STATUS.md`;
  `rg -n "[ \t]+$" gamedesign/meme-evolution/cow_evolution_deconstruction_v2.md tasks/active/T0011-research-cow-evolution-loop-and-balance-for-67-w.md tasks/STATUS.md` returned no matches.

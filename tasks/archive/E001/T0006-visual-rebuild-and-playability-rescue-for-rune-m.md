---
id: T0006
title: Visual rebuild and playability rescue for Rune Marches native slice
status: dropped
epic: E001
priority: P0
tags: [visual, ux, ftue, native, rescue, review]
created: 2026-06-13
updated: 2026-06-15
---

## What

Stop content expansion and rescue the native Rune Marches slice so it looks and
plays like an intentional casual fantasy RPG prototype, not a debug UI over a
cropped fake shot.

The current runtime screenshots pass automation but fail the product bar:
visual hierarchy is noisy, landmarks/labels overlap, buttons read as generic
debug slabs, the journal is cramped, and the first-session flow is hard to
understand without an agent explaining it.

## Done when

- [ ] A visual/product review report names the current failures, root causes,
  and replacement direction.
- [ ] Native runtime first screen has one clear player goal, one dominant
  primary action, readable status, and no overlapping labels in 960x540 and
  360x640 screenshots.
- [ ] UI controls use a coherent game UI kit: fewer buttons on the default
  screen, icon-first affordances, distinct combat vs exploration state, and no
  debug-looking button grid.
- [ ] World map is rebuilt or heavily cleaned so landmarks are readable and do
  not look like cropped artifacts pasted over a background.
- [ ] FTUE path is shortened for the first audience test; deeper content can
  remain in state/DevAPI but must not overload the first screen.
- [ ] Native scenario and playtest probe pass after the visual/FTUE rebuild.
- [ ] Desktop and portrait screenshots are reviewed visually and attached as
  evidence.

## Open questions

- Should the rescue pass create a new generated UI/map sheet, or should it
  first simplify the existing runtime composition and only then regenerate
  assets?
- How much content should be hidden behind the first-session path so the build
  feels playable instead of like a feature checklist?

## Log

- 2026-06-13: Created after lead review rejected the current native visuals as
  not matching the casual Skyrim goal. Review scope: screenshots
  `tmp/rune_marches/native_branch_landmark_labeled.png`,
  `tmp/rune_marches/native_branch_landmark_portrait.png`, art direction
  `gamedesign/projects/rune-marches/art/art_direction.md`, passive profile
  `tmp/session_profiles/session_profile_2026-06-14.summary.md`.
- 2026-06-15: Dropped during pipeline cleanup: Rune Marches visual rescue is legacy product work and should not appear as current P0 work.

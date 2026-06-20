---
id: T0022
title: Reference evidence refresh after first mech screenshot
status: done
epic: ""
priority: P1
tags: [gamedesign, references, evidence, review, screenshots, mechs]
created: 2026-06-19
updated: 2026-06-20
---

## What

After the first native PC screenshot sequence exists, refresh the central
reference deconstructions and compare the actual build against the intended
Mech Arena / CATS / Mechangelion translation.

Scope boundaries:

- In scope: current native capture paths, mismatch audit, reference digest
  update, screenshot evidence board, and next proof target.
- Out of scope: new gameplay features, final art generation, economy tuning,
  and exact UI copying before evidence is recorded.

## Design inputs

- `gamedesign/projects/mech-builder-battler/references/mech_arena_deconstruction_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/references/cats_deconstruction_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/references/mechangelion_deconstruction_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/design/reference_readiness_and_prototype_plan_2026-06-19.md`

## Done when

- [x] The first native screenshot sequence is linked from the project wiki or
      an evidence folder.
- [x] Each central reference doc names the current build capture or explicitly
      explains why it is missing.
- [x] Mismatch audit lists concrete gaps in first screen, controls, combat
      response, reward, upgrade, and visual composition.
- [x] The next implementation pass is tied to one screenshot/DevAPI proof.
- [x] `node tools/taskboard/cli.mjs validate` passes or any failure is logged.

## Open questions

- Do we need owned gameplay captures from installed reference games before
  tuning exact pacing, or are official/store/support sources enough for the
  first prototype review? Answer for this task: not needed to close the
  current-build mismatch audit, but still required before copying exact pacing,
  UI, economy, or live-reference details.

## Log

- 2026-06-19: Created as the research follow-up that becomes possible only
  after the first runtime screenshot exists.
- 2026-06-19: Added
  `gamedesign/projects/mech-builder-battler/references/current_build_mismatch_audit_2026-06-19.md`,
  linked the first native screenshot sequence from the project wiki, and updated
  the Mech Arena, CATS, and Mechangelion deconstructions with current capture
  paths, concrete mismatches, and next native proof targets. Validation:
  `node tools/taskboard/cli.mjs validate`.
- 2026-06-20: Post-prototype cleanup: archived as historical Mech Builder Battler work after the user stopped the game.

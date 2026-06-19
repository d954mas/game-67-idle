---
id: T0014
title: First slice spec for Mech Builder Battler
status: review
epic: ""
priority: P1
tags: [gamedesign, spec, mvp, mobile, web, mechs]
created: 2026-06-19
updated: 2026-06-19
---

## What

Create a production-facing first vertical slice spec for `mech-builder-battler`
from the current GDD and reference packets. The spec should define the concrete
hangar -> battle -> reward -> upgrade -> second battle prompt loop, screen
contracts, first combat rules, first content list, data concepts, tuning
targets, fake-shot requirements, and acceptance criteria.

Scope boundaries:

- In scope: design/spec document, first-slice decisions, implementation gates,
  proof sequence, and lead review questions.
- Out of scope: runtime implementation, pipeline/tools/engine changes, final
  art generation, exact balance numbers, and exact UI/economy copying from
  incomplete reference packets.

## Done when

- [x] `gamedesign/projects/mech-builder-battler/design/first_slice_spec_2026-06-19.md`
      exists with frontmatter and links to the current GDD/reference docs.
- [x] The spec defines fixed/deferred decisions, player flow, screen contracts,
      combat rules, content list, data concepts, tuning targets, fake-shot
      requirements, acceptance criteria, and review questions.
- [x] The spec preserves gates for native PC slice scope, orientation, accepted
      fake shots, and stronger source evidence.
- [x] `node tools/taskboard/cli.mjs validate` passes or any failure is logged.

## Open questions

- This spec is the implementation source together with the accepted decision
  handoff.

## Log

- 2026-06-19: Added first vertical slice spec at
  `gamedesign/projects/mech-builder-battler/design/first_slice_spec_2026-06-19.md`.
  Kept task in `review` because the next step is lead review and fake-shot or
  native PC slice-scope acceptance.

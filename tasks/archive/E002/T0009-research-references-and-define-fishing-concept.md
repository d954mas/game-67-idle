---
id: T0009
title: Research references and define fishing concept
status: done
epic: E002
priority: P0
tags: [gdd, references, concept, profiling]
created: 2026-06-15
updated: 2026-06-15
---

## What

Create the first research packet for the fishing game before implementation:
Definition of Done, reference lock, source matrix, initial visual/gameplay
synthesis, and the small set of lead questions needed to lock the concept.

## Done when

- [x] `gamedesign/projects/roblox-fishing/concept.md` states the current DoD,
      working concept, assumptions, and lead questions.
- [x] `gamedesign/projects/roblox-fishing/references/fishing_reference_study.md`
      records sources, borrow/avoid/copy-risk, current-build mismatch, and
      implementation readiness.
- [x] `tmp/roblox_fishing_profile.md` records profiling notes for the first
      research pass.
- [x] Final report states whether the reference gate is ready, partial, or
      blocked before any code/art implementation.

## Open questions

- Answered 2026-06-15: casual audience; progression/grind good; complex
  gameplay bad; feel and fake shot are important; progression clarity matters;
  realism forbidden; visuals should be bright, juicy, pleasant, and noticeable.

## Log

- 2026-06-15: Started. Loaded `primary-gdd-pipeline`,
  `game-visual-art-direction`, `task-manager`, and profiling policy. Research
  is intentionally stopping before code/final art for lead discussion.
- 2026-06-15: Added concept draft, partial reference study, visual direction
  brief, and profiling notes. Validation: `node tools/taskboard/cli.mjs
  validate` passed.
- 2026-06-15: Lead answered direction questions. Reference gate remains
  partial for implementation, but is accepted for GDD/fake-shot direction.
- 2026-06-15: Added stronger gameplay evidence from a Fisch screenshot
  walkthrough, Fish It/rod progression guides, and captured current native
  mismatch at `tmp/roblox_fishing/current_native_before_fishing.png`. Reference
  gate is ready enough for first native prototype after fake shot review.

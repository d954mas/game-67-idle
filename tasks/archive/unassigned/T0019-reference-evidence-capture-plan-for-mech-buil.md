---
id: T0019
title: Reference evidence capture plan for Mech Builder Battler
status: done
epic: ""
priority: P1
tags: [gamedesign, references, evidence, capture-plan, mobile, web, mechs]
created: 2026-06-19
updated: 2026-06-20
---

## What

Create a project-specific evidence capture plan for `mech-builder-battler` that
defines what screenshots, video timestamps, observation ledgers, friction
examples, and native PC mismatch checks are still needed before exact
reference-driven UI/combat/economy/art claims can drive implementation.

Scope boundaries:

- In scope: reference evidence plan, source candidate list, evidence board
  requirements, per-reference capture questions, observation ledger template,
  native PC mismatch template, and implementation readiness verdict.
- Out of scope: runtime implementation, pipeline/tools/engine changes, actual
  frame capture, final art generation, and claiming implementation-ready
  evidence before boards are filled.

## Done when

- [x] `gamedesign/projects/mech-builder-battler/references/reference_evidence_capture_plan_2026-06-19.md`
      exists with frontmatter and links to current reference/design docs.
- [x] The plan defines capture requirements for Mech Arena, CATS,
      Mechangelion, and War Robots.
- [x] The plan includes evidence board requirements, observation ledger
      template, native PC mismatch template, source candidates, and acceptance
      criteria.
- [x] The plan states that current references are not ready for exact
      implementation and names what can proceed now.
- [x] `node tools/taskboard/cli.mjs validate` passes or any failure is logged.

## Open questions

- Who will capture the first gameplay/screenshot boards: agent with browsing,
  user-provided media, or later manual capture?
- Should War Robots remain anti-pattern only, or should its PvE/solo mode be
  studied as a secondary combat reference?
- Should Mechangelion be demoted if stronger gameplay evidence is not found?

## Log

- 2026-06-19: Added reference evidence capture plan at
  `gamedesign/projects/mech-builder-battler/references/reference_evidence_capture_plan_2026-06-19.md`.
  Kept task in `review`: the plan is complete, but the evidence boards
  themselves still need capture before exact implementation claims are ready.
- 2026-06-20: Post-prototype cleanup: archived as historical Mech Builder Battler work after the user stopped the game.

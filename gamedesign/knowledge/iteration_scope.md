---
type: Game Design Knowledge
title: Iteration Scope
description: Rules for keeping playable iterations small and reviewable.
tags: [scope, iteration, planning]
timestamp: 2026-06-13T00:00:00Z
---

# Iteration Scope

Reusable rules for cutting game work into small playable iterations.

## Goal

An iteration should improve a player-visible slice of the game, not just move
internal tasks forward. Scope is good when it can be built, played, reviewed,
and either kept, cut, or revised quickly.

## Slice Shape

```text
One player goal
  -> one primary action
  -> one system response
  -> one visible reward or consequence
  -> one validation path
```

## Core Principles

- Prefer a small complete loop over a large partial system.
- Define what the player can do at the end of the iteration.
- Keep the first implementation path boringly direct.
- Add polish only after the loop, feedback, and validation path exist.
- Cut secondary screens, currencies, content, and edge cases until the slice proves value.
- Every iteration should produce evidence: screenshot, playthrough, test, log, or telemetry.
- Do not call a task done if it exists in code but cannot be understood in play.

## Planning Checklist

- What player-visible problem does this iteration solve?
- What is the smallest playable path through it?
- Which knowledge files apply?
- What is explicitly out of scope?
- What placeholder is acceptable for this pass?
- What must be real because a placeholder would invalidate the test?
- What evidence will prove the iteration worked?
- What can be cut if time runs short?
- What follow-up decision should this iteration unlock?

## Definition Of Done

- The build starts.
- The target flow can be reached from a fresh state or documented fixture.
- The player can perform the intended action.
- The game responds with visible feedback.
- The state changes correctly.
- The next goal or repeat action is clear.
- Logs do not show critical runtime errors.
- At least one screenshot or scripted playthrough proves the flow.
- Known issues are written down with severity.

## Scope-Cut Rules

- Cut a new feature before weakening the core loop.
- Cut extra content before cutting feedback.
- Cut cosmetic variants before cutting readability.
- Cut optional UI before cutting the primary action.
- Cut late-game systems before weakening the first session.
- Cut automation only if manual validation remains fast and reliable.

## Anti-Patterns

- Building several systems until all are half usable.
- Accepting a feature because it compiles.
- Adding tutorial text instead of making the first action clearer.
- Starting with full data models before testing one player path.
- Implementing rare edge cases before the common loop works.
- Mixing refactor, content expansion, balance, and visual polish in one vague task.

## Validation

- A tester can start the slice without private designer knowledge.
- One scripted or manual path covers the main loop.
- The first screenshot shows what changed compared with the previous iteration.
- The next priority is obvious from the review findings.
- The iteration can be reverted or cut without corrupting unrelated systems.

## Links

- Use [Core Loop](core_loop.md) to identify the smallest playable loop.
- Use [GDD Application](gdd_application.md) to write the slice spec.
- Use [Design Review](design_review.md) before implementation.
- Use [Playtest Validation](playtest_validation.md) after implementation.
- Use [Telemetry Evidence](telemetry_evidence.md) to define proof before building.


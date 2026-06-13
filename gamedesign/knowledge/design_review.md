---
type: Game Design Knowledge
title: Design Review
description: Checklist for reviewing game design docs before implementation.
tags: [review, gdd, quality]
timestamp: 2026-06-13T00:00:00Z
---

# Design Review

Reusable checklist for reviewing a GDD section before implementation.

## Goal

A design review should catch unclear behavior, missing feedback, weak first-session
flow, risky scope, and untestable requirements before code or asset production starts.

## Review Order

```text
Player goal
  -> core loop
  -> first-time experience
  -> feedback and rewards
  -> progression and balance
  -> UI and visual readability
  -> content scope
  -> validation
```

## Severity

- Blocker: cannot implement safely because behavior, state, or validation is undefined.
- High: implementable but likely to fail comprehension, loop, feedback, or pacing.
- Medium: understandable but incomplete, weak, or likely to cause rework.
- Low: polish, naming, formatting, or minor clarity issue.

## Review Checklist

- Player goal: is the player-facing purpose clear?
- Core loop: does the feature strengthen or intentionally change a loop?
- First-time use: is the first encounter with this feature specified?
- Action: does the player know what to do?
- Response: does the game response have concrete rules?
- Feedback: are success, error, blocked, ready, and completed states covered?
- Reward: does the reward change state, capability, visuals, or goals?
- Progression: does the feature fit a visible unlock or pacing path?
- Balance: are tuning knobs separate from fixed rules?
- UI: are component states, hierarchy, and edge cases defined?
- Visuals: are silhouettes, readability, and asset roles clear?
- Content: is the minimum content set defined?
- Scope: can the feature be cut or staged?
- Validation: can a tester or automation prove it works?

## Review Comments Format

```md
### Finding
Severity: Blocker | High | Medium | Low
Symptom: What is unclear or risky.
Cause: Why this will cause implementation or product problems.
Fix: Concrete change to the GDD.
Validation: How to verify the fix.
```

## Common Findings

- The feature is a screen, not a loop.
- The reward is a number but has no visible result.
- The first-time state is not specified.
- Locked content has no unlock reason.
- UI states are implied but not listed.
- Balance target is missing, so tuning has no direction.
- Content requirements are too broad for the current milestone.
- Validation depends on designer opinion instead of observable evidence.

## Done Criteria

- No blocker findings remain.
- High findings are fixed or explicitly accepted as risk.
- The project GDD includes player behavior, system behavior, feedback, tuning, content, and validation.
- Unknowns are listed as open questions.
- The implementation task can be scoped into a small playable iteration.

## Links

- Use [GDD Application](gdd_application.md) to rewrite weak sections.
- Use [Core Loop](core_loop.md) for loop findings.
- Use [Reward Feedback](reward_feedback.md) for reward and feedback findings.
- Use [Balance Tuning](balance_tuning.md) for pacing findings.
- Use [Content Planning](content_planning.md) for scope findings.
- Use [Playtest Validation](playtest_validation.md) for evidence criteria.
- Use [Iteration Scope](iteration_scope.md) to cut the next implementation pass.
- Use [Telemetry Evidence](telemetry_evidence.md) to make validation observable.
- Use [Game Feel And Controls](game_feel_controls.md) for input and moment-to-moment feel.
- Use [Release Readiness](release_readiness.md) when reviewing an external-test candidate.

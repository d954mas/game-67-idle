---
type: Game Design Knowledge
title: Telemetry Evidence
description: Reusable rules for logs, events, screenshots, and proof artifacts.
tags: [telemetry, evidence, validation]
timestamp: 2026-06-13T00:00:00Z
---

# Telemetry Evidence

Reusable rules for defining logs, events, and evidence in game design specs.

## Goal

Telemetry and logs should help the team answer product questions quickly:
did the player understand, did the loop complete, did the reward arrive, and
where did the session fail? Evidence should be designed with the feature, not
added only after debugging becomes painful.

## Evidence Types

- Screenshot: proves visual state and readability.
- Video or recording: proves motion, timing, and interaction sequence.
- Scripted input: proves a flow is repeatable.
- Runtime log: proves startup, errors, warnings, and state transitions.
- Telemetry event: proves player actions and system outcomes.
- Balance trace: proves economy and pacing over time.
- Playtest note: captures confusion, hesitation, delight, and intent.

## Event Design Principles

- Track player intent, not only button clicks.
- Pair start events with completion, cancel, fail, or timeout events.
- Include enough context to explain the event without replaying the whole session.
- Keep event names stable and readable.
- Do not log private data or unnecessary identifiers.
- Define events for first-session gates before tuning later funnels.
- Prefer a few high-signal events over noisy exhaust.

## GDD Checklist

- What product question must this evidence answer?
- Which player action starts the measured flow?
- What counts as success?
- What counts as failure, cancel, timeout, or blocked state?
- Which state fields are needed to diagnose the outcome?
- Which screenshot or recording should accompany the event?
- Which logs should be checked when the event fails?
- What is the expected event order for the happy path?
- What is the minimum evidence needed for release candidate review?

## Event Naming Pattern

```text
area.feature.action.phase

Examples:
session.start
core_loop.action.started
core_loop.action.completed
reward.claimed
upgrade.purchase.blocked
screen.opened
```

Use project-specific names in the project GDD. Keep this file generic.

## Useful Properties

- `session_id`
- `build_id`
- `elapsed_seconds`
- `screen`
- `feature`
- `action_id`
- `result`
- `reason`
- `resource_before`
- `resource_after`
- `progress_before`
- `progress_after`
- `input_method`
- `error_code`

## Anti-Patterns

- Logging only success events.
- Logging every frame or every UI hover without a question to answer.
- Using event names that depend on temporary copy.
- Adding telemetry after the feature is already ambiguous.
- Treating telemetry as a replacement for screenshots and playtests.
- Capturing data that cannot affect a design, balance, or stability decision.
- Leaving logs inaccessible to the agent or tester running the build.

## Validation

- A first-session playthrough produces the expected event order.
- Failure paths produce a reason, not just silence.
- Screenshots and logs can be matched to the same run.
- Event names are understandable without reading code.
- The event set answers the review questions in [Playtest Validation](playtest_validation.md).
- Sensitive or unnecessary data is not captured.

## Links

- Use [Playtest Validation](playtest_validation.md) to define evidence needs.
- Use [Core Loop](core_loop.md) to define loop start and completion events.
- Use [Balance Tuning](balance_tuning.md) for pacing traces and milestone timing.
- Use [Iteration Scope](iteration_scope.md) to keep evidence small and actionable.


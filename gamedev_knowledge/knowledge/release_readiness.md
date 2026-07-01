---
type: Game Design Knowledge
title: Release Readiness
description: Reusable checklist for external testing and release-candidate readiness.
tags: [release, qa, validation]
timestamp: 2026-06-13T00:00:00Z
---

# Release Readiness

Reusable design-side checklist for deciding whether a game build is ready for an
external test, soft launch, portal review, or release-candidate pass.

## Goal

Release readiness is not the same as feature completion. A build is ready for
external testing when the first experience is understandable, the core loop works,
progression has a reason to continue, critical feedback is readable, and known
risks are documented.

## Readiness Layers

```text
Launch
  -> first screen
  -> first action
  -> first loop completion
  -> first reward
  -> first progression step
  -> return reason
  -> stability evidence
```

## Design Checklist

- The first screen communicates the player fantasy and next action.
- The core loop can be completed from a fresh start.
- The first reward is visible and desirable.
- The first progression step changes state, capability, or world visuals.
- The player always has a useful next action during the first session.
- Locked content explains one clear unlock reason.
- UI text is readable on the smallest target viewport.
- Primary controls work on all target input methods.
- Major errors, blocked actions, purchases, rewards, and unlocks have feedback.
- The game has enough content for the intended test duration.
- The build has a clear stop point, return hook, or next-goal state.

## Evidence Checklist

- Fresh-start screenshot.
- First-action screenshot or recording.
- First reward/upgrade screenshot.
- Scripted or manual playthrough notes for the first loop.
- Logs from a clean launch and at least one completed loop.
- Known issues with severity.
- Balance trace or milestone timing for first-session pacing.
- Device/viewport/input coverage note.
- Accessibility risk note.

## No-Go Conditions

- Crash, black screen, broken input, or impossible progression.
- First action is unclear without explanation.
- First reward is invisible or feels unrelated to the action.
- Core loop requires hidden knowledge to complete.
- UI blocks or hides the primary action.
- Critical logs show repeated runtime errors.
- Save/load or session recovery corrupts progress where persistence matters.
- Content or copy violates audience, platform, or safety constraints.

## Conditional Go

A build can still be testable with known issues when:

- The issue is documented with severity.
- The first loop remains understandable.
- The issue does not block progression.
- The tester can avoid or recover from it.
- The next iteration has a clear fix priority.

## Anti-Patterns

- Calling a build ready because all planned tasks are checked off.
- Shipping a technically complete build with a confusing first minute.
- Treating visual polish as optional when readability depends on it.
- Ignoring logs because manual play seemed fine once.
- Adding content instead of fixing the first loop.
- Hiding known issues in chat instead of the project docs.

## Links

- Use [Playtest Validation](playtest_validation.md) for first-minute and first-session checks.
- Use [Iteration Scope](iteration_scope.md) to decide what must be in the RC slice.
- Use [Telemetry Evidence](telemetry_evidence.md) to define proof.
- Use [Accessibility](accessibility.md) and [Mobile/Web Platform Design](mobile_web_platform.md) for platform risk.
- Use [Design Review](design_review.md) before declaring readiness.


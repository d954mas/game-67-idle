# Playtest Validation

Reusable validation checklist for early casual game builds.

## Goal

Playtesting should answer whether the game is understandable, playable, rewarding,
and stable in the first minutes. It is not enough that the build runs or that the
features exist in code.

## Validation Layers

```text
Smoke
  -> the game starts and accepts input
Comprehension
  -> the player understands what to do
Core loop
  -> one complete loop works and changes state
Feedback
  -> actions, rewards, errors, and unlocks are visible
Progression
  -> short-term and medium-term goals exist
Stability
  -> no critical crashes, stalls, or broken states
```

## First-Minute Checklist

- Can the player identify the primary action from a still screenshot?
- Does the first action happen within 5 seconds?
- Does the game give visible feedback immediately after input?
- Is the first reward reached within 15-30 seconds?
- Does the player know what changed after the reward?
- Is there only one dominant next action before the loop is understood?
- Can the player recover from an accidental tap or wrong choice?

## First-Session Checklist

- The player completes at least one full core loop.
- The player reaches a new decision, upgrade, unlock, or goal.
- The player sees at least one visible reward, not only a number change.
- The player understands why locked content is locked.
- The player never gets stuck with no useful action.
- The game avoids long waits before the player understands why waiting matters.
- The session ends with a clear next goal or return reason.

## Evidence To Capture

- Screenshot of first screen.
- Screenshot after first action.
- Screenshot after first reward or upgrade.
- Short input script or recorded steps for the first loop.
- Logs for startup, errors, warnings, asset failures, and crashes.
- Balance trace or telemetry for loop starts, completions, rewards, spends, exits.
- Notes from at least one cold playtest or simulated cold-player pass.

## Bug Severity

- Critical: crash, black screen, broken input, impossible progression, save corruption.
- High: player cannot understand first action, reward is invisible, UI blocks core loop.
- Medium: confusing copy, weak feedback, poor pacing, unclear locked state.
- Low: polish issue, minor visual mismatch, non-blocking copy or layout defect.

## Director Review Questions

- What did the player try first?
- What did the player ignore?
- Where did the player wait, hesitate, or misread the screen?
- Which reward felt good enough to repeat the loop?
- Which system exists technically but does not yet work as a product?
- What is the smallest change that improves the next test?
- What should be cut or hidden until the loop is stronger?

## Anti-Patterns

- Marking a build ready because it launches.
- Testing only with the developer who already knows the design.
- Accepting a feature because the code path works, while the screen does not explain it.
- Adding more tutorial text instead of improving action hierarchy and feedback.
- Ignoring logs when screenshots look acceptable.
- Testing late-game systems before the first loop is stable.
- Treating balance as numbers only, without checking player-visible pacing.

## Links

- Use [Core Loop](core_loop.md) to define what one complete loop means.
- Use [FTUE](ftue.md) to validate first-session comprehension.
- Use [UI/UX Patterns](ui_ux_patterns.md) to validate readability and state clarity.
- Use [Meta Progression](meta_progression.md) to validate rewards, unlocks, and pacing.
- Use [Reward Feedback](reward_feedback.md) to validate reward visibility.
- Use [Visual Direction](visual_direction.md) to validate visual appeal and readability.
- Use [Balance Tuning](balance_tuning.md) to compare player pacing with targets.
- Use [Content Planning](content_planning.md) to decide what to cut, hide, or add.
- Use [Telemetry Evidence](telemetry_evidence.md) to define logs, events, traces, and screenshots.
- Use [Mobile/Web Platform Design](mobile_web_platform.md) and [Accessibility](accessibility.md) for viewport, input, and readability checks.

## References

- Nielsen Norman Group, [10 Usability Heuristics](https://www.nngroup.com/articles/ten-usability-heuristics/)
- Game Accessibility Guidelines, [Full list](https://gameaccessibilityguidelines.com/full-list/)
- W3C WAI, [WCAG 2.2 Target Size Minimum](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html)

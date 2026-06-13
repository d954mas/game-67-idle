---
type: Game Design Knowledge
title: Core Loop
description: Reusable structure for defining and reviewing the main gameplay loop.
tags: [core-loop, gameplay, validation]
timestamp: 2026-06-13T00:00:00Z
---

# Core Loop

Reusable structure for defining and reviewing the main repeatable gameplay loop.

## Goal

The core loop is the smallest repeatable cycle that makes the game playable:
the player acts, the game responds, the player receives value, and a new reason
to continue appears. If this loop is unclear, no amount of meta, content, UI, or
polish will make the game feel ready.

## Basic Shape

```text
Player intent
  -> player action
  -> system response
  -> immediate feedback
  -> reward or progress
  -> new option, goal, risk, or upgrade
  -> repeat with a changed state
```

## Core Principles

- The player should understand what they are trying to do before optimizing it.
- The first loop completion should be fast enough to test in seconds, not minutes.
- The reward should be visible before it becomes abstract.
- The next loop should start from a changed state.
- A good loop creates a reason to repeat, not just a requirement to repeat.
- The loop should support both short-term feedback and long-term progression.
- Avoid using variable rewards or grind as a substitute for a meaningful action.
- Every added system should either strengthen the loop or intentionally create a new loop layer.

## GDD Checklist

- Player fantasy: what does repeating the loop make the player feel like they are doing?
- Start state: what does the player have before the loop begins?
- Action: what input or decision advances the loop?
- Cost: what does the action consume, risk, or block?
- Response: what does the game calculate or change?
- Feedback: what animation, sound, number, or state change confirms the action?
- Reward: what value does the player gain?
- Next state: what new action, goal, or decision becomes available?
- Failure or miss: what happens when the player acts poorly, late, or wrongly?
- Exit: when should the player stop repeating this loop and do another loop?
- Scaling: how does the loop stay interesting after 1, 10, and 100 repetitions?

## Loop Layering

```text
Moment loop
  -> input, reaction, reward
Session loop
  -> short goal, unlock, completion
Meta loop
  -> long-term upgrade, collection, area, prestige, mastery
```

Each layer should feed another layer. A moment loop that never changes the
session state becomes busywork. A meta loop that does not improve the moment
loop becomes spreadsheet progression.

## Anti-Patterns

- The player repeats an action only because there is nothing else to do.
- The reward is only a number and has no visible or felt impact.
- The loop has a long cooldown before the player understands why it matters.
- A new system interrupts the loop before the first completion.
- The loop generates resources faster than there are meaningful sinks.
- Failure stops play instead of creating a recovery action.
- The best strategy is obvious after one repetition and never changes.
- The loop is fun in the GDD but has no concrete input, feedback, or reward spec.

## Validation

- A tester can perform one complete loop without explanation.
- A still screenshot makes the next action and current reward understandable.
- The first completion produces visible feedback and changed state.
- The second completion is not identical in player meaning to the first.
- The loop works before meta progression is layered on top.
- Telemetry or debug logs can count loop starts, completions, rewards, exits, and failures.
- If the loop is repeated for several minutes, the player reaches at least one new decision.

## Links

- Use [Playtest Validation](playtest_validation.md) to prove the loop works in a build.
- Use [Reward Feedback](reward_feedback.md) to make loop consequences visible.
- Use [Game Feel And Controls](game_feel_controls.md) to make repeated actions responsive.
- Use [FTUE](ftue.md) to introduce the first loop without overload.
- Use [Meta Progression](meta_progression.md) to connect loop rewards to long-term goals.
- Use [UI/UX Patterns](ui_ux_patterns.md) to make loop state and feedback readable.

## References

- Hunicke, LeBlanc, Zubek, [MDA: A Formal Approach to Game Design and Game Research](https://users.cs.northwestern.edu/~hunicke/MDA.pdf)
- Wikipedia, [MDA framework](https://en.wikipedia.org/wiki/MDA_framework)
- Wikipedia, [Compulsion loop](https://en.wikipedia.org/wiki/Compulsion_loop)

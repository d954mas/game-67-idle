---
type: Game Design Knowledge
title: Meta Progression
description: Reusable patterns for long-term progression, unlocks, and goals.
tags: [meta-progression, economy, rewards]
timestamp: 2026-06-13T00:00:00Z
---

# Meta Progression

Reusable structure for long-term progression, economy, unlocks, and rewards.

## Goal

Meta progression turns repeated core-loop play into long-term goals. It should
give the player reasons to return, spend, optimize, unlock, and feel that the
game world has changed because of their actions.

## Loop Layers

```text
Core loop
  -> minute-to-minute action and reward
Session loop
  -> goals completed during one play session
Meta loop
  -> upgrades, unlocks, collections, areas, builds, prestige, or account growth
```

## Core Principles

- Every meta reward should change visible state, available actions, or future pacing.
- Early upgrades should answer "what changed?" without requiring math.
- Add currencies only when each currency has a distinct job.
- Keep the first progression ladder short, visible, and fast.
- Use locked content as motivation only when the unlock condition is clear.
- Sources and sinks must both be explicit.
- Avoid permanent choices before the player understands their value.
- Parallel goals are useful after the player understands the main loop, not before.
- Prestige or reset systems need a clear power fantasy before they are offered.

## GDD Checklist

- Core resource: what does the player earn most often?
- Source: what actions create the resource?
- Sink: what actions remove or spend the resource?
- First upgrade: what is bought first, and what becomes better?
- Unlock ladder: what opens at each early milestone?
- Visible result: what changes on screen after each upgrade type?
- Pacing: expected time to first reward, first spend, first unlock, first slowdown.
- Soft block: what should the player do when they cannot afford the next item?
- Return hook: what grows while away, or what goal waits for the next session?
- Tuning knobs: costs, income, timers, caps, unlock thresholds, reward multipliers.
- Failure case: how does the economy recover from over-generous rewards or dead ends?

## Common Patterns

- Milestone ladder: a short chain of obvious goals with visible rewards.
- Upgrade shop: repeatable spending that improves core-loop speed or yield.
- Area unlocks: new screens or zones that represent progress physically.
- Collection sets: items that create medium-term completion goals.
- Timed jobs: delayed rewards that give the player a reason to return.
- Build/base growth: upgrades change the home, map, character, or environment.
- Soft reset: later-game reset for permanent benefit after current progression slows.

## Anti-Patterns

- Early `+2%` rewards with no visible or felt difference.
- Many currencies before the player has a reason to distinguish them.
- Locked content with no unlock condition.
- Rewards that only delay the next interesting decision.
- Sinks that feel like taxes instead of choices.
- Meta goals that compete with the FTUE's only next action.
- Infinite price scaling without new visual milestones.

## Validation

- The first three purchases each create a visible or clearly felt change.
- A new player can name the next unlock without opening a wiki or external guide.
- Economy simulation covers the first session and at least one longer return loop.
- The player always has a useful short-term goal, even when they cannot afford the next big item.
- No resource accumulates forever without a sink, unless accumulation is the intended fantasy.
- No sink removes progress without a clear benefit, compensation, or consent.

## Links

- Use [Core Loop](core_loop.md) to confirm meta rewards improve repeated play.
- Use [Playtest Validation](playtest_validation.md) to check pacing, stuck states, and reward clarity.
- Use [Reward Feedback](reward_feedback.md) to make purchases, unlocks, and upgrades visible.
- Use [Balance Tuning](balance_tuning.md) to tune costs, rewards, pacing, and sinks.
- Use [Content Planning](content_planning.md) to plan unlockable items, areas, and goals.
- Use [FTUE](ftue.md) for when and how progression enters the first session.
- Use [UI/UX Patterns](ui_ux_patterns.md) for upgrade states, affordance, and reward feedback.

## References

- GEEvo paper, [Game Economy Generation and Balancing with Evolutionary Algorithms](https://arxiv.org/abs/2404.18574)
- Wikipedia, [Incremental game](https://en.wikipedia.org/wiki/Incremental_game)
- Wikipedia, [Virtual economy](https://en.wikipedia.org/wiki/Virtual_economy)

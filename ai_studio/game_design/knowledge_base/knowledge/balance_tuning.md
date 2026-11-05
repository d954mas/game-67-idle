---
type: Game Design Knowledge
title: Balance Tuning
description: Reusable pacing and economy checks for game balance.
tags: [balance, economy, progression]
timestamp: 2026-11-05T00:00:00Z
---

# Balance Tuning

Reusable rules for tuning game numbers, pacing, economy, and difficulty.

## Goal

Balance should create the intended player experience, not just mathematically
valid numbers. A useful balance spec states target pacing, player-visible
effects, tuning knobs, and validation evidence.

## Balance Targets

- Time to first action.
- Time to first reward.
- Time to first spend.
- Time to first unlock.
- Time to first meaningful choice.
- Time to first slowdown.
- Session length target.
- Return, retry, or re-entry pacing target, if the game has one.
- Failure/recovery time.

## Core Principles

- Tune toward a player experience target before adjusting individual numbers.
- Prefer a few explicit tuning knobs over many hidden coefficients.
- Early balance should be felt, not merely calculated.
- A cost increase needs a corresponding increase in anticipation or reward value.
- Avoid dominant strategies unless they are intentional early guidance.
- Use simulations to find obvious pacing failures, then playtests to judge feel.
- Keep sources, sinks, caps, timers, multipliers, and unlock thresholds visible in the spec.
- When changing a number, state which player behavior should change.

## GDD Checklist

- What is the target first-session timeline?
- What are the main resources, sources, and sinks?
- Which numbers are fixed rules and which are tuning knobs?
- What progression curve is expected: linear, stepwise, exponential, capped, hybrid?
- What is the intended bottleneck at each phase?
- What happens when the player plays faster than expected?
- What happens when the player plays slower than expected?
- What does the player do when they cannot afford the next thing?
- What is the minimum viable simulation or spreadsheet?
- Which telemetry events prove pacing in a live build?

## Simulation Checklist

- Start from a fresh player state.
- Simulate the first minute, first session, and at least one return session.
- Track resources earned, resources spent, unlock times, failure/retry time, and blocked time.
- Print milestone times in player-readable labels.
- Flag impossible purchases, dead-end states, runaway growth, and unused resources.
- Compare simulation output against the GDD target timeline.
- Re-run after every economy-affecting change.

## First-Session Bot Report

A deterministic bot that plays a fresh save answers pacing questions a playtest
cannot repeat. It plays faster than a person, so its times are a lower bound; a
person's gaps stretch by roughly the same factor, so judge the shape.

- Report by time window: purchases made, median and longest gap between
  purchases. A long gap is where the player has nothing to want.
- Report when everything is bought: the minute of the last purchase, the share of
  the run's total content banked at that moment, and unspent currency at the end.
- With a repeating run (rebirth, new world), print the same for each run, and save
  and reload between runs so the ledger is checked too.
- Compare before and after every price change: same route seed, same policy, one
  knob, in one table. Read the first window on its own. A retune can fix the end
  of the run and ruin the opening, and the totals will not show it.
- Keep optional rewards out of the baseline: the bot watches no ads. Measure a
  reward's uplift in a separate report-only run, and never assert it.
- A bot stall is a bot limit until a person confirms it. Do not read it as a
  design fact.
- These numbers are design knobs: print them, do not pin them in tests.

## Anti-Patterns

- Tuning by changing many numbers at once.
- Balancing only for the designer's known optimal route.
- Using long waits before the player understands the loop.
- Adding a new currency to fix unclear rewards.
- Hiding a bad core loop behind generous rewards.
- Making early upgrades mathematically correct but visually meaningless.
- Ignoring the slow player path because the fast path works.

## Validation

- First-session milestones land within the target ranges.
- A cold tester reaches at least one visible upgrade or unlock.
- No resource grows forever without a reason.
- No sink blocks progress without creating a decision.
- The intended bottleneck is visible and understandable.
- Telemetry and playtest notes agree on where players slow down.

## Links

- Use [Core Loop](core_loop.md) to identify what balance is pacing.
- Use [Meta Progression](meta_progression.md) for economy structure.
- Use [Reward Feedback](reward_feedback.md) to verify tuned rewards are visible.
- Use [Playtest Validation](playtest_validation.md) to compare simulation with player feel.

## References

- GEEvo paper, [Game Economy Generation and Balancing with Evolutionary Algorithms](https://arxiv.org/abs/2404.18574)
- Wikipedia, [Game balance](https://en.wikipedia.org/wiki/Game_balance)

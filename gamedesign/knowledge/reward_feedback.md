---
type: Game Design Knowledge
title: Reward Feedback
description: Reusable rules for making rewards, progress, and consequences visible.
tags: [rewards, feedback, ux]
timestamp: 2026-06-13T00:00:00Z
---

# Reward Feedback

Reusable rules for making rewards, progress, and consequences feel clear.

## Goal

Every important action should answer three player questions immediately:
what did I do, what did I get, and why should I care? Rewards that are only
stored in state but not shown to the player do not yet work as product design.

## Reward Ladder

```text
Input response
  -> immediate local feedback
  -> resource or progress change
  -> visible world/UI change
  -> next goal or new possibility
```

## Reward Types

- Resource: currency, energy, materials, points.
- Power: higher yield, faster action, new ability, automation.
- Access: new area, mode, character, job, card, or upgrade tier.
- Cosmetic: object, outfit, decoration, environment improvement.
- Social/status: rank, badge, title, collection completion.
- Narrative: new reaction, scene, line, event, relationship, mystery.

## Core Principles

- Put feedback close to the source of the action before updating distant counters.
- Make the first rewards concrete and visible before using abstract multipliers.
- Pair numbers with motion, sound, color, or object changes when the reward matters.
- New unlocks should explain both what opened and why it is useful.
- A purchase should visibly change the player's capability, world, or options.
- Avoid permanent reward spam; strong feedback is best when it has contrast.
- Use distinct feedback for success, blocked, error, ready, and completed states.
- Do not use random rewards to hide weak core actions.

## GDD Checklist

- What is the reward for the action?
- Where does the reward appear first?
- Which counter, object, map area, or component changes?
- What does the player see within 0.1 seconds of input?
- What does the player see within 1 second of reward?
- Does the reward unlock a new decision or only increase a number?
- How is a blocked action different from a successful action?
- How is a rare or major reward different from a common reward?
- What feedback is optional, muted, or reduced for accessibility?
- What telemetry proves reward delivery and reward visibility?

## Anti-Patterns

- Reward text appears far from the action source and is easy to miss.
- The only feedback is a counter changing silently.
- The same sparkle, sound, or color is used for every outcome.
- A major upgrade gives a tiny percentage bonus with no visible result.
- Multiple reward effects compete and hide the next action.
- A reward animation blocks input longer than the player wants.
- The game celebrates before the player understands what happened.

## Validation

- A tester can describe what changed after each major action.
- A screenshot after reward delivery shows changed state.
- Major rewards are distinguishable from common rewards.
- Blocked/error feedback is clear without reading logs.
- Feedback remains readable on the smallest supported viewport.
- Repeated rewards stay satisfying without hiding controls or goals.

## Links

- Use [Core Loop](core_loop.md) to connect rewards to repeatable play.
- Use [Meta Progression](meta_progression.md) to connect rewards to long-term goals.
- Use [Balance Tuning](balance_tuning.md) to align reward size with pacing.
- Use [UI/UX Patterns](ui_ux_patterns.md) for component states and layout.
- Use [Playtest Validation](playtest_validation.md) to verify reward clarity in builds.

## References

- Nielsen Norman Group, [10 Usability Heuristics](https://www.nngroup.com/articles/ten-usability-heuristics/)
- Game Accessibility Guidelines, [Full list](https://gameaccessibilityguidelines.com/full-list/)
- Wikipedia, [Compulsion loop](https://en.wikipedia.org/wiki/Compulsion_loop)

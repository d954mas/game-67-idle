---
id: QDES_COMMON
name: Game Design Common
group: game_design
description: Use first when gameplay, progression, economy, player motivation, challenge, rewards, or playable-slice design changed and you need a cheap pass for obvious design blockers.
---

# Game Design Common

Use this first when changed work affects gameplay, progression, economy,
features, player motivation, challenge, rewards, or playable-slice design.

## What It Checks

Catches obvious gameplay design blockers before spending time on numbered
game-design checks.

## Use When

Gameplay, progression, economy, features, player motivation, challenge, rewards,
or playable-slice design changed.

## Do Not Use For

- GDD/document clarity by itself;
- player-facing screen clarity by itself;
- art direction or asset readiness;
- runtime/build behavior.

## Check

- the player has a clear action to take;
- the action creates feedback or progress;
- repetition has a reason, variation, or decision;
- rewards support the intended loop;
- the feature fits the current game direction.

If any item fails, fix the design before using numbered game-design checks.

## Evidence

Use a design doc section, playable run, scenario, screenshot/video, prototype
state, or task link.

## Not Enough

- A build-only check.
- A feature list with no player action or feedback.
- A reward claim with no visible progress or reason to continue.

## Record As

```text
Quality: QDES_COMMON=pass; evidence: <doc, prototype, or task link>
```

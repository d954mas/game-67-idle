---
id: QDES_001
name: Playable Loop
group: game_design
description: Use when gameplay, design data, economy, progression, reward, challenge, or first-slice work changes the player's action loop.
---

# QDES_001 Playable Loop

## What It Checks

The gameplay model forms a player loop: action, response, visible
progress/reward, repeat reason, and next hook.

## Use When

Use when gameplay, design data, economy, progression, reward, challenge, first
playable slice, repetition, or live gameplay work changes what the player does,
what changes in response, why it matters, or why the player continues.

This can apply to docs, tables, JSON/data contracts, prototypes, or runtime
changes. It does not require the change to be implemented yet.

## Do Not Use For

- GDD/design-source clarity by itself;
- player-facing clarity by itself;
- art direction or asset readiness;
- runtime/build behavior.

## Check

- What does the player do?
- What changes in response?
- What reward or progress is visible?
- Why would the player repeat the action?
- What is the next hook?
- What blocked, failure, recovery, or re-entry state keeps the loop from
  becoming confusing or dead?
- Does the feature fit the current game direction?

## Evidence

Design data/table, scenario, playable run, screenshot/video, task log, or review
evidence.

## Not Enough

- Build or launch success without playable-loop evidence.
- A pretty screen with no repeatable player action.
- A reward/economy claim with no visible feedback or progress.
- A design table or JSON contract that lists rewards but not the player action,
  response, repeat reason, or next hook.

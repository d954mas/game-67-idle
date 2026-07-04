---
type: Game Design Document
title: RB Dark RPG GDD
description: Implementation-facing game design document for RB Dark RPG.
tags: [gdd]
game_id: rb-dark-rpg
status: draft
---

# RB Dark RPG GDD

Status: draft

## Definition Of Done

First design pass is done when the project has a runnable starter build, a
single first-playable slice target, structured loop/UI data, and an explicit
list of decisions that need lead input before combat implementation.

## Player Fantasy

You are a fragile but capable scavenger-knight entering a cursed border ruin,
where each room asks whether to press deeper, recover, or retreat.

## First 30 Seconds

The player spawns outside a ruined gate, sees one interactable objective and one
visible danger lane, moves with WASD, opens settings from the starter UI, and can
reach a first encounter marker without reading instructions.

## Core Loop

See `data/core_loop.json`.

## Player Verbs

- Move
- Inspect
- Engage
- Retreat
- Recover
- Upgrade

## Rules And Feedback

The first slice should prove movement, a single enemy or hazard, a readable
health/stamina signal, and one reward or recovery decision. Every failure must
show why it happened.

## UI Flow

See `data/ui_flow.json`.

## Assets And Visual Proof

See `data/asset_manifest.json`.

## Validation

- Native starter build launches with the `RB Dark RPG` title.
- DevAPI smoke still reaches UI tree, render info, and game-state endpoints.
- First playable later needs a screenshot or capture of the gate plus encounter
  marker at desktop and narrow viewport sizes.

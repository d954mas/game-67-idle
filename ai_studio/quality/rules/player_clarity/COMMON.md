---
id: QCLR_COMMON
name: Player Clarity Common
group: player_clarity
description: Use first when player-facing output changed and you need a cheap pass for blank or broken output, visible actions, important scene objects, overlap, and actual-output proof.
---

# Player Clarity Common

Use this first when changed work affects player-facing UI, HUD, screenshots,
canvas surfaces, gameplay scenes, sprites, page layout, interactive elements, or
feedback.

## What It Checks

Catches obvious player-facing clarity blockers before spending time on numbered
checks.

## Use When

Any player-facing output changed: screen, HUD, gameplay scene, sprite visibility,
feedback, page layout, or interactive element placement.

## Do Not Use For

- art direction, style, composition, or polish;
- asset license, provenance, or runtime format;
- input event delivery, focus, keybinds, pointer capture, or platform input;
- core loop, rewards, progression, or motivation.

## Check

- the main screen or gameplay scene is not blank or broken;
- important text fits and is readable on the target surface;
- primary actions are visible on the target surface;
- important scene objects, sprites, goals, rewards, or danger are visible;
- interactive elements do not overlap or mislead on the target surface;
- the result is checked from actual output, not only source code.

If any item fails, fix it before using numbered player-clarity checks.

## Evidence

Use a screenshot, running surface, video, runtime capture, or short observation
from actual output.

## Not Enough

- Source reading without looking at output.
- A screenshot that does not show the changed state.
- A claim that the screen is clear without visible evidence.

## Record As

```text
Quality: QCLR_COMMON=pass; evidence: <screenshot or runtime proof>
```

---
id: QCLR_001
name: Screen Clarity
group: player_clarity
description: Use when a player-facing screen, gameplay scene, onboarding step, feedback state, or task flow changed and the player must understand what is happening, what changed, and what to do next.
---

# QCLR_001 Screen Clarity

## What It Checks

Checks whether the player can understand the current screen or gameplay state
and choose the next action from actual output.

## Use When

A player-facing screen, gameplay scene, HUD, menu, onboarding step, feedback
state, or task flow changed.

## Do Not Use For

- art direction, style, composition, or polish;
- asset license, provenance, or runtime format;
- input event delivery, focus, keybinds, pointer capture, or platform input;
- core loop, rewards, progression, or motivation.

## Check

- Where am I?
- What can I do now?
- What changed after the action?
- Why does it matter?
- What should I do next?

## Evidence

Use actual output: screenshot, running surface, video, or a short observation
from the current player-facing state.

## Not Enough

- Code compiles but no screen was checked.
- One screenshot that does not show the changed state.
- Agent explanation that the screen is clear without visible evidence.

For verification-only review, return CONFIRM/REFUTE rather than broad
commentary.

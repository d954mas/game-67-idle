---
id: QCLR_001
name: Player Clarity
group: player_clarity
description: Use when changed player-facing state or feedback can make the current situation or next action ambiguous.
---

# QCLR_001 Player Clarity

## What It Checks

A changed player-facing state does not make the current situation or next
action ambiguous.

## Use When

Use when a new or changed screen, HUD state, gameplay feedback, onboarding
step, empty/error/loading state, state transition, or interactive flow can make
the player misread what happened, what matters now, or what to do next.

## Do Not Use For

- pure responsive geometry;
- pure art direction, style, composition, or polish;
- technical/rendering invariants without changed player-facing state;
- asset license, provenance, or runtime format;
- input event delivery, focus, keybinds, pointer capture, or platform input;
- code-only changes with no changed player-facing state;
- core loop, rewards, progression, or motivation.

## Check

- output is not blank, broken, or missing the changed state;
- important text is readable and fits its surface;
- primary actions are visible;
- player-relevant objects, state, danger, or feedback are visible;
- interactive elements do not overlap or mislead;
- the player can answer: where am I, what happened, what matters, what next?

## Evidence

Screenshot, running surface, video, runtime capture, or short observation from
the current player-facing state.

## Not Enough

- Code compiles but no screen was checked.
- One screenshot that does not show the changed state.
- Agent explanation that the screen is clear without visible evidence.

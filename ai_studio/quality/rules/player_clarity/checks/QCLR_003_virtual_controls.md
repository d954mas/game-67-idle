---
id: QCLR_003
name: Virtual Controls
group: player_clarity
description: Use when mobile/touch controls, virtual buttons, joystick zones, or control hints can be hidden, too small, overlapping, misleading, or unreachable.
---

# QCLR_003 Virtual Controls

## What It Checks

On-screen mobile controls are visible, understandable, reachable, and do not
block important gameplay or HUD information.

## Use When

Use when mobile/touch controls, virtual buttons, joystick zones, or on-screen
control hints can be hidden, too small, overlapping, misleading, or unreachable.
If control layout depends on viewport or orientation, also use
[QCLR_002](QCLR_002_responsive_viewports.md).

## Do Not Use For

- keyboard input, touch/pointer event delivery, focus, or pointer capture;
- general viewport/orientation layout without virtual controls;
- art direction, composition, or polish;
- asset license, provenance, or runtime format;
- core loop, rewards, progression, or motivation.

## Check

- virtual buttons and joystick zones are visible on supported mobile layouts;
- controls are large enough to understand and tap;
- controls do not cover important gameplay, HUD, state, feedback, or danger;
- control labels/icons match the visible action;
- portrait and landscape control layouts are usable when both are supported;
- the player can understand how to control the game on touch devices.

## Evidence

Screenshots, runtime capture, UI bounds/inspected state, or a short observation
that names the exact mobile viewport/orientation and control state inspected.
Include both mobile portrait and mobile landscape unless one is explicitly
unsupported.

## Not Enough

- Engine touch/input tests without visible control layout evidence.
- One orientation screenshot when both portrait and landscape are supported.
- Visible controls that hide important gameplay or HUD information.

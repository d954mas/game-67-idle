---
id: QCLR_002
name: Responsive Viewports
group: player_clarity
description: Use when 4:3, 16:9, or tall-phone viewport ratios can crop, hide, overlap, or misplace player-facing layout.
---

# QCLR_002 Responsive Viewports

## What It Checks

Required viewport ratios keep the changed game surface, HUD, text, and
player-facing actions visible and correctly placed.

## Use When

Use when viewport ratio, orientation, player-facing layout placement, or
responsive sizing can crop, hide, overlap, or misplace player-facing layout.
If virtual controls affect layout or safe space, also use
[QCLR_003](QCLR_003_virtual_controls.md).

## Do Not Use For

- input event delivery, focus, keybinds, pointer capture, or platform input;
- virtual buttons, joystick zones, or touch control hints when they do not
  affect viewport layout or safe space;
- art direction, composition, or polish;
- asset license, provenance, or runtime format;
- state/feedback ambiguity without layout risk.

## Check

- 4:3 layout is usable in landscape and portrait;
- 16:9 layout is usable in landscape and portrait;
- tall-phone 19.5:9 layout is usable in landscape and portrait;
- important HUD, text, state, feedback, or danger are not cropped or pushed off-screen;
- player-facing actions are not hidden, overlapped, or misplaced.

For a local surface change, check only the affected surface and the supported
targets that can break. Use the full 4:3, 16:9, and tall-phone 19.5:9
landscape/portrait matrix when root layout, HUD shell, viewport scaling, safe
area, or global responsive behavior changed.

## Evidence

Screenshot or runtime UI bounds for the affected supported targets. Full-matrix
evidence means 4:3, 16:9, and tall-phone 19.5:9 in landscape and portrait,
unless the project explicitly does not support one of them.

## Not Enough

- One landscape screenshot when portrait is supported.
- One phone screenshot when 4:3 or 16:9 layouts are also supported.
- Compressed desktop HUD reused as mobile layout.

---
id: QCLR_002
name: Responsive Layout
group: player_clarity
description: Use when viewport, target surface, responsive layout, or click/touch target geometry changed and player-facing actions must remain visible, reachable, and non-overlapping.
---

# QCLR_002 Responsive Layout

## What It Checks

Checks whether player-facing layout and click/touch targets stay usable across
the target surfaces.

## Use When

Viewport, target surface, responsive layout, click/touch target geometry,
portrait, desktop, tablet, or touch layout changed.

## Do Not Use For

- input event delivery, focus, keybinds, pointer capture, or platform input;
- art direction, composition, or polish;
- asset license, provenance, or runtime format.

## Check

- primary actions remain visible on each target surface;
- click/touch targets are reachable;
- interactive elements do not overlap or mislead;
- visible button art matches the actual click/touch target;
- important HUD, text, goals, rewards, or danger are not pushed off-screen.

## Evidence

When runtime UI bounds are available, use them as evidence. Otherwise use
screenshots from the target surfaces.

## Not Enough

- One desktop screenshot when portrait is in scope.
- Visible button art with wrong clickable bounds.
- Compressed desktop HUD reused as mobile layout.

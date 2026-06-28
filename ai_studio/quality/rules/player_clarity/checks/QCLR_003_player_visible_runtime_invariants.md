---
id: QCLR_003
name: Player-Visible Runtime Invariants
group: player_clarity
description: Use when runtime visual code changed and could break player-facing text rendering, debug/final visual boundaries, or Y-up/Y-down layout invariants.
---

# QCLR_003 Player-Visible Runtime Invariants

## What It Checks

Checks hard runtime invariants that affect what the player sees.

## Use When

Active game runtime visual code changed and could touch text rendering, final
rendering paths, layout coordinates, platform/input/devapi boundaries, or debug
visuals.

## Do Not Use For

- general screen clarity when runtime invariants did not change;
- art direction, composition, or polish;
- asset license, provenance, or runtime format;
- input event delivery, focus, keybinds, pointer capture, or platform input.

## Check

Player-visible runtime code must not use handmade text, debug renderers as final
visuals, or Y-down layout conventions outside platform/input/devapi boundaries.

## Evidence

Use source inspection and runtime evidence for the changed files.

## Not Enough

- Debug debt hidden in final presentation.
- Handmade glyph tables for product-facing UI.
- Y-down conversions outside boundary files.

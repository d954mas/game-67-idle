---
id: QART_001
name: Art Direction Fit
group: art
description: Use when generated art, composition, material feel, visual polish, final presentation screenshots, or an accepted visual target changed.
---

# QART_001 Art Direction Fit

## What It Checks

Checks whether the output fits the accepted visual target, composition,
material feel, polish bar, and current game's style.

## Use When

Work changes generated art, composition, material feel, visual polish,
screenshots as final presentation, or an accepted visual target.

## Do Not Use For

- player-facing clarity, layout, or click/touch targets;
- asset license, provenance, or runtime format;
- runtime/build behavior;
- game-loop or progression design.

## Check

Inspect the actual runtime or generated output. Do not accept claims based only
on source files.

Look for:

- placeholder/debug visuals;
- weak composition or unclear action direction;
- mismatch with accepted visual target;
- material, palette, silhouette, or style choices that make the result feel
  cheaper or off-target for the current game.

## Evidence

Use screenshots, references, accepted visual targets, generated source images,
or runtime captures. Do not accept claims based only on source files.

## Not Enough

- Source reading without looking at output.
- A screenshot with no accepted target or comparison when target fit is claimed.
- Treating player-clarity issues as art approval.

If text, interactive elements, or screen flow fail, apply Player Clarity rules
too. This rule judges art direction fit, not player clarity by itself.

If a visual direction was rejected, do not continue feature/content expansion on
top of it without explicit lead acceptance.

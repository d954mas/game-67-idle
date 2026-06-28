---
id: QART_COMMON
name: Art Common
group: art
description: Use first when art, generated images, visual polish, materials, screenshots, or final-looking player output changed and you need a cheap pass for obvious art blockers.
---

# Art Common

Use this first when changed work affects art, generated images, visual polish,
3D presentation, materials, screenshots, or final-looking player output.

## What It Checks

Catches obvious art and presentation blockers before spending time on numbered
art checks.

## Use When

Art, generated images, visual polish, 3D presentation, materials, screenshots,
or final-looking player output changed.

## Do Not Use For

- player-facing clarity, layout, or click/touch targets;
- asset license, provenance, or runtime format;
- runtime/build behavior;
- game-loop or progression design.

## Check

- output is not placeholder/debug-looking unless explicitly draft;
- composition has a clear subject and readable action;
- assets are not visibly broken, stretched, missing, or flat by accident;
- style does not contradict the accepted visual target when one exists;
- review is based on actual visual output.

If any item fails, fix it before using numbered art checks.

## Evidence

Use screenshots, generated source images, accepted/rejected references, runtime
captures, or a short observation from actual output.

## Not Enough

- Source reading without looking at output.
- A technical build pass without visual evidence.
- A claim that art is improved without a target or visible comparison.

## Record As

```text
Quality: QART_COMMON=pass; evidence: <screenshot or artifact path>
```

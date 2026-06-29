---
id: QART_001
name: Closest Practical Visual
group: art
description: Use when player-facing visuals can default to debug shapes, placeholder art, or temporary overlays instead of the closest practical final-direction visual.
---

# QART_001 Closest Practical Visual

## What It Checks

Player-facing visuals use the closest practical version of the intended final
direction instead of defaulting to debug shapes, placeholder art, or temporary
overlays.

## Use When

Use when adding, replacing, generating, adapting, or sourcing visual candidates
for player-facing visuals, sprites, UI art, screenshots, or visual targets.

## Do Not Use For

- player-facing clarity, layout, or click/touch targets;
- asset license, provenance, or runtime format;
- runtime/build behavior;
- game-loop or progression design.

## Check

- the agent attempted the closest practical visual option first: create, source,
  or adapt before falling back to debug or placeholder visuals;
- debug or placeholder visuals are used only after explicit prototype/spike
  request, real source/asset gap after source attempt, or lead approval before
  implementation;
- if the task did not explicitly allow rough prototype/spike output and the
  agent cannot make, source, or adapt a closer practical visual, the agent asks
  the lead before using debug or placeholder visuals;
- actual visual output was inspected, not only code or claims;
- output is not broken, stretched, cropped, missing, or accidentally flat.

## Evidence

Use visual evidence when judging output: screenshot, generated source image,
runtime capture, or visual target/reference.

Use source/code evidence only to explain why debug/placeholder visuals were
used: short create/source/adapt attempt note, asset gap, lead approval, or code
path showing the visual is intentionally temporary.

## Not Enough

- Source reading without looking at output.
- Debug shapes or placeholder art used by default without a create, source, or
  adapt attempt.
- Debug shapes or placeholder art used without explicit prototype scope,
  source/asset gap after source attempt, or lead approval.
- Treating player-clarity issues as art approval.

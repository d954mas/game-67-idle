---
type: Art Direction
title: Rune Marches Visual Direction
description: Visual target and asset guidance for generated/free assets.
tags: [art, assets, fake-shot, ui, visual-direction]
timestamp: 2026-06-13T00:00:00Z
---

# Rune Marches Visual Direction

## User Visual Direction

Generated and free assets are allowed. File size matters, but visual quality is
more important for this project.

## Accepted Direction Target

- File: `gamedesign/projects/rune-marches/art/fake_shots/rune-marches-gameplay-v1.png`
- Role: visual target and composition reference, not a runtime asset.
- Status: accepted as direction for mood, palette, landmark readability, and
  panel/icon treatment.

## What To Keep

- Warm hamlet vs cool marsh contrast.
- Painted low-fantasy world with readable landmark silhouettes.
- Large map nodes that imply open-world travel without requiring free 3D
  movement.
- Purple Mire Wisp enemy with clear silhouette and magic identity.
- Dark metal/wood UI panels with gold trim, but simplified for mobile.
- Big icon-first action buttons and progression nodes.

## What To Simplify Before Runtime Integration

- Reduce top inventory density for FTUE. First minute should show HP, mana,
  silver, XP, road safety, and one goal only.
- Do not bake labels, numbers, quest text, damage values, or button text into
  reusable UI backgrounds.
- Keep runtime text and state values separate for localization, balance, and
  accessibility.
- Avoid exact Elder Scrolls signals: dragons, shouts, horned helmets, compass
  copy, or copied faction/quest iconography.
- Avoid exact The Quest UI layout copying.

## Runtime Asset Priorities

1. Painted map background for Miregate/Wispfen/Old Bell Tower.
2. Separate landmark icons for Miregate, Wispfen Road, Old Bell Tower locked,
   and Old Bell Tower unlocked.
3. Mire Wisp enemy sprite with transparent background.
4. Spark spell effect sprite.
5. Slice9 panel and button backgrounds: idle, disabled, affordable, selected.
6. Resource icons: HP, mana, silver, XP, rune spark, road safety.

## Runtime Integration V1

- Generator: `tools/assets/build_rune_marches_assets.py`.
- Runtime assets: `assets/runtime/rune-marches-v1/`.
- Crop/slice manifest:
  `gamedesign/projects/rune-marches/data/rune-marches-v1-crop_manifest.json`.
- Native proof:
  `tmp/rune_marches/native_first_slice_labeled.png` and
  `tmp/rune_marches/native_first_slice_portrait_current.png`.

This is a first audience-test visual pass: it proves generated bitmap assets in
the native runtime, but some sprites still originate from fake-shot crops. Before
release-quality art, replace those with clean transparent source sheets and add
separate resource icons.

## Size Policy

- Prefer WebP or compressed PNG for large backgrounds when web build starts.
- Keep source PNGs in project art folders; generate runtime-compressed copies
  in `assets/runtime/rune-marches-v1/`.
- Do not block visual quality for the first audience test merely to minimize
  size; optimize after the target look is proven.

## Generation Prompt Used

```text
Generate a polished gameplay fake shot for an original casual fantasy RPG
called Rune Marches, 16:9 PC/web game screenshot. Scene: readable
first-session UI for a casual Skyrim-like open world RPG, not copying Elder
Scrolls. Painted stylized low-fantasy art, warm hamlet on left, blue-green
marsh road in center, locked old bell tower on right, small glowing purple Mire
Wisp enemy panel, magic spark upgrade theme. Layout: top status bar with simple
icon slots (no readable text, no letters), center map with three landmarks,
right quest/combat panel, bottom large touch-friendly buttons with icon
placeholders only, no baked labels or numbers. Mood: inviting, mysterious,
child-safe, high visual clarity, mobile-readable silhouettes, cozy village
light vs cold marsh magic. No logos, no watermarks, no readable text, no
dragons, no horned helmet, no copyrighted UI.
```

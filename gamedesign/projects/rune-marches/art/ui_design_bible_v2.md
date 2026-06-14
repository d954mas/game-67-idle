---
type: Art Bible
title: Rune Marches UI Asset Bible V2
description: Reusable generated UI kit rules for Rune Marches runtime assets.
tags: [art, ui, ux, slice9, assets, rune-marches]
timestamp: 2026-06-14T00:00:00Z
---

# Rune Marches UI Asset Bible V2

## Purpose

This bible turns generated UI art into reusable native runtime assets. The
source sheet is not a gameplay screenshot; it is a cuttable kit for panels,
buttons, bars, chips, icon frames, and resource icons.

## Source Sheet

- Accepted source:
  `gamedesign/projects/rune-marches/art/source_sheets/rune-marches-ui-kit-source-v2.png`
- Generator mode: built-in image generation.
- Role: source family for reusable UI, not final runtime file.
- Background: flat magenta chroma key.
- Runtime output:
  `assets/runtime/rune-marches-ui-map-rescue-v2/`
- Crop/runtime manifests:
  `gamedesign/projects/rune-marches/data/rune-marches-ui-map-rescue-v2-crop_manifest.json`
  and
  `gamedesign/projects/rune-marches/data/rune-marches-ui-map-rescue-v2-asset_manifest.json`

## Visual Rules

- Mood: readable low-fantasy UI, polished but restrained.
- Materials: dark walnut wood centers, charcoal iron rails, compact antique
  gold corners, teal marsh-magic accent gems.
- Palette:
  - walnut panel center: `#2b1b12` to `#4a2c1b`
  - iron trim: `#25262a` to `#4a4d55`
  - antique gold: `#b48332` to `#f1c96a`
  - marsh teal accent: `#19b8b0` to `#77f0e5`
  - rune purple accent: `#8e48d9` to `#d88cff`
- Border language: compact decorated corners, simple long edges, clean centers.
- Text is always runtime-rendered. No labels, numbers, quest text, or resource
  values belong in reusable backgrounds.

## UI Component Rules

### Panels

- Use for objective/journal/combat containers.
- Keep content inside the recorded `content` safe area.
- Decorative corner gems are allowed only outside the content area.
- Stretch by 9-slice geometry; do not uniform-scale the whole panel.

### Buttons

- Use the wide blank button backgrounds only.
- Runtime composes icon and label above the button.
- States:
  - idle: warm gold trim, dark wood center
  - pressed/selected: slightly brighter gold trim
  - disabled: gray iron/low saturation
- Avoid placing important text over corner caps.

### Status Bars And Chips

- Status bar is a long slice9 strip for HP/mana/silver/XP row.
- Reward chip is a compact slice9 component for one reward or unlock callout.

### Icons

- Resource icons are isolated runtime sprites, not baked into buttons.
- Minimum gameplay preview: 32 px tall.
- Preferred source role:
  - heart: health
  - blue droplet: mana
  - silver coin: silver
  - gold star: XP/progression
  - teal shield: road safety
  - purple rune: rune spark/magic upgrade

## Slice9 Policy

- Every slice9 asset records `slice9`, `content`, and
  `target_preview_sizes`.
- Preview sizes are chosen to catch impossible margins and bad stretching:
  minimum, normal first-screen, and large desktop.
- If edge ornamentation creates obvious stretching artifacts, regenerate a
  cleaner edge or split corners/edges into separate source pieces.

## Responsive Composition Rules

Desktop and portrait layouts are separate compositions built from the same
slice9 assets. Portrait must not be a compressed desktop HUD.

- Desktop may show the full status strip: HP, MP, silver, XP, road safety, map,
  journal, and three bottom choices.
- Portrait shows only the immediately useful status values in the first
  viewport: HP, MP, silver, and level/XP. Road safety and route details move
  into later context or larger screens.
- Portrait gives the primary action its own full-width row. Secondary choices
  sit below it as two half-width buttons.
- Portrait journal copy must be short: one heading, one progress read, and one
  next-action line. Dense side/east/green/briar/moon stat strings are desktop
  only.
- The first portrait screen must answer player-read questions without zooming:
  where am I, what can I do, what changed, and why continue.
- Generated UI kits are not accepted until desktop and portrait screenshots
  both prove that slice9 assets compose without overlap or unreadable text.

## Rejection Rules

Reject or regenerate if any of these appear:

- readable text, fake letters, numbers, labels, or watermark;
- icons fused into button backgrounds;
- buttons or panels touching each other so crops are ambiguous;
- decorative corners intrude into recorded content safe areas;
- center texture too busy for runtime text;
- inconsistent button state color language;
- copied Elder Scrolls compass/dragon/shout/horned-helmet signals.

## QA Notes From V2 Source

- Contact sheet evidence:
  `gamedesign/projects/rune-marches/art/previews/rune-marches-ui-kit-v2-contact-sheet.png`
- Slice9 preview evidence:
  `gamedesign/projects/rune-marches/art/previews/rune-marches-ui-kit-v2-slice9-preview.png`
- Pixel audit evidence:
  `gamedesign/projects/rune-marches/reviews/generated_ui_asset_audit_v2.md`
- Responsive layout audit evidence:
  `gamedesign/projects/rune-marches/reviews/responsive_layout_audit_ui_kit_v2_desktop.md`
  and
  `gamedesign/projects/rune-marches/reviews/responsive_layout_audit_ui_kit_v2_portrait.md`
- The source sheet is good enough to validate the reusable-asset pipeline:
  separate blank panels/buttons, isolated icons, no baked labels, real crop and
  slice metadata.
- It is not final art approval. Current visible issues:
  - V2 initially had partial icon crops and magenta/purple fringe; the builder
    now uses border-connected chroma cleanup, icon component isolation, alpha
    trim, padding, and the generated UI pixel audit;
  - source gutters are still tighter than a production icon sheet should allow;
    future generated sheets should separate icons farther apart before
    slicing;
  - status bar and icon frame have ornamentation that stretches awkwardly at
    some preview sizes;
  - portrait layout now uses a dedicated primary action row and compact HUD,
    but release UI still needs a cleaner mobile HUD treatment and less ornate
    status strip art;
  - release pass should regenerate cleaner long edges or split decorative caps
    from stretchable centers.

## Prompt Used

```text
Create a clean source sheet for an original casual fantasy RPG UI kit called
Rune Marches. The sheet must contain separated blank reusable UI parts only:
one large modal panel frame, one smaller journal panel, three blank wide button
backgrounds (idle, pressed, disabled), one status bar strip, one small reward
chip, one square icon frame, and six isolated simple resource icons
(heart/health, mana droplet, silver coin, XP star, road safety shield, rune
spark). No readable text, no letters, no numbers, no labels.

Polished painterly low-fantasy game UI, mobile-readable, dark carved wood and
muted iron frames with restrained warm gold trim, subtle teal marsh magic
accents, child-safe but not cartoonish. 2048x2048 square asset sheet,
orthographic front view, each asset separated by generous uniform gutters,
aligned to an invisible grid, no overlaps, no perspective skew. Put all items
on a perfectly flat solid magenta #ff00ff chroma-key background for later
cutting. Blank panels and buttons only; runtime text and values will be added
by code. Corners and borders must be visually suitable for 9-slice stretching:
corners decorative but compact, long edges simple and repeatable, center areas
clean.
```

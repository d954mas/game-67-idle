---
id: T0008
title: Generated visual direction and UI art pipeline for fishing
status: done
epic: E002
priority: P0
tags: [visuals, ui, generated-art, assets, profiling]
created: 2026-06-15
updated: 2026-06-15
---

## What

Produce an accepted visual target and generated-art/UI pipeline packet for the
fishing game. The work should prove reusable UI/assets, not only one attractive
flat mockup.

## Done when

- [x] Art bible defines palette, materials, silhouettes, UI shape language, and
      no-go motifs.
- [x] One fake gameplay shot is generated or assembled and reviewed by the
      lead before expanding.
- [x] Runtime asset checklist separates source art, cropped/runtime assets,
      UI bases, icons, fish/cards, and world assets.
- [x] Profiling notes capture generation/tool friction and what improved the
      skill workflow.

## Open questions

- Should first visual proof be a full gameplay fake shot, a UI kit source
  sheet, or a 3D world style board?

## Log

- 2026-06-15: Lead accepted casual/simple/progression/juicy/no-realism
  direction. Generated first fake shot:
  `gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1.png`.
  It needs lead review before runtime asset generation. Generated prompt
  packets for blank UI kit, icons, and decor overlay source families.
- 2026-06-15: Produced a runtime-usable partial UI/icon slice from
  `splash-rods-ui-icons-source-v2-magenta-clean.png` into
  `assets/runtime/roblox-fishing-ui-v1/` and wired it into the native prototype.
  This is accepted as first proof only; final UI kit still needs cleaner source
  separation and crop validation.
- 2026-06-15: Lead review completed through the fishing prototype test. The
  fake shot remains useful as target/evidence, but the fishing game is now
  closed and no final UI kit pass will be pursued for Splash Rods.
- 2026-06-15: Closed with fishing test iteration; generated visual direction and partial UI pipeline evidence retained for E003 pipeline review.

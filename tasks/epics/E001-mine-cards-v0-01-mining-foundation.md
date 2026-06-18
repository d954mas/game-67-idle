---
id: E001
title: Mine Cards v0.01 mining foundation
status: active
priority: P1
tags: [mine-cards, idle, mining, prototype]
created: 2026-06-17
updated: 2026-06-17
---

## Goal

Build the first native Mine Cards slice around one Mining idle loop, using the
Melvor-inspired tiny-start strategy and the accepted blocky mining direction.

The epic is successful when a new player can open the native screen and
understand:

- what is currently running;
- what reward is being earned;
- what upgrade is next;
- why to keep mining for the next few minutes.

## In scope

- v0.01 Mining-only economy and first-session tuning;
- public-safe 3D miner direction and first runtime asset source;
- one native Mining activity screen;
- real asset render path, not debug shape visuals;
- production skeletal animation path planning and first native proof through a
  reusable extension/module beside the engine;
- modular mesh-part animation only as a fallback if skeletal rendering blocks
  the first playable screen;
- screenshot/readability/product gates for the first screen.

## Out of scope

- card runs;
- combat;
- Smithing and full equipment grids;
- offline progress;
- broad Melvor-scale skill matrix;
- web/browser prototype;
- full armor/clothing production pipeline beyond the first skinned/attached
  gear proof.

## Log

- 2026-06-17: Created after base GDD review to split the Mine Cards v0.01 work
  into balance, asset-path, native-screen, and optional skeletal-spike tasks.
- 2026-06-17: Lead chose the production skeletal path after the sidecar showed
  ready KayKit `Pickaxing` data. v0.01 planning now targets ozz-animation
  runtime proof first, with mesh-part animation retained only as fallback.
- 2026-06-17: Lead clarified the production skeletal path must be a reusable
  extension/module outside `external/neotolis-engine`, not an engine submodule
  patch.

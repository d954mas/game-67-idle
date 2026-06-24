---
id: T0114
title: Add richer Blockside Heat NPC pursuit beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, npc, pursuit]
created: 2026-06-23
updated: 2026-06-23
---

## What

Add one richer NPC pursuit beat after the vehicle-feel pass: the guard should
create clearer pressure during the package route without adding a new district
or broad police system.

## Done when

- [x] Pursuer behavior has at least one visible escalation beyond direct chase.
- [x] Player has a readable response using the existing toy blaster or vehicle
      movement.
- [x] Native capture/probe evidence covers pursuit pressure, player response,
      and reward/recovery.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0113 vehicle-feel pass. Scope is one NPC pursuit
  beat, not full police AI.
- 2026-06-23: Added roadblock escalation during the package route:
  `roadblock_active`, `roadblock_cleared`, pursuit grace timing, red roadblock
  markers, and toy-blaster clearing response. Evidence:
  `tmp/blockside-heat/pursuit-pressure-latest.png`,
  `tmp/blockside-heat/pickup-stress-latest.png`, and
  `tmp/blockside-heat/capture-states-report.json`. Product gate remains pass
  in `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-12-17-661Z_desktop.md; screenshot: tmp/blockside-heat/pursuit-pressure-latest.png; evidence: NPC pursuit PASS: tmp/blockside-heat/capture-states-report.json includes pursuit_pressure roadblock_active=true/wanted=1 and pickup_stress roadblock_cleared=true/pursuer_stunned=true; screenshots: tmp/blockside-heat/pursuit-pressure-latest.png and tmp/blockside-heat/pickup-stress-latest.png; gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json; build PASS: cmake --build --preset native-debug.; next: Next narrow slice can add a second street job or a small story/mission intro.

---
id: T0003
title: Playable UI slice
status: done
epic: E001
priority: P1
tags: [fantasy-pocket-rpg, ui]
created: 2026-06-11
updated: 2026-06-11
---

## What

Player can click through the first expedition and understand the next goal.

## Done when

- [x] province map visible
- [x] encounter actions visible
- [x] camp actions visible
- [x] result feedback visible

## Open questions

## Log

- 2026-06-11: Seeded from implementation_tasks.json phase list.
- 2026-06-11: Runtime gained clickable native UI zones and visual panels. Initial screenshot evidence used `build/captures/fantasy_slice_full_loop.png`; later review replaced this with framebuffer capture evidence.
- 2026-06-11: Added readable native text, result feedback, scene stamps, and game-side framebuffer capture. Evidence: `py -3.12 tools/devapi/capture_demo.py 9125 build/captures/fantasy_slice_text_full_loop.png --full-loop`, `py -3.12 tools/devapi/agent_playtest.py 9126 --full-loop`, screenshot `build/captures/fantasy_slice_text_full_loop.png`.

---
id: T0265
title: "Canvas animation card v1 (video route): anim card + generateAnimFromCard -> editable element.flipbook + rAF flipbook player; inc2 animation MODE w/ timeline trim/delete/fps/play-mode; inc3 FLF multi-keyframe"
status: todo
project: P001
epic: E010
priority: P1
tags: [canvas, animation, video]
created: 2026-07-04
updated: 2026-07-04
---

## What

Build the Canvas video-route animation card: describe animation, generate a
flipbook element, preview it on canvas, then expand toward timeline editing and
multi-keyframe FLF.

## Done when

- [ ] Increment 1 creates an anim card and generates editable flipbook output.
- [ ] The canvas preview uses the existing rAF loop without procedural transforms.
- [ ] Later increments for timeline editing and FLF are accepted or split.

## Open questions

## Log
- 2026-07-04: Lead decisions 2026-07-05: matte default = corridorkey; EXPORT = frame-image SET (uniform size, aligned by the fixed video canvas - NO per-frame offsets/pivots stored; sheet packing stays optional; per-frame trim+offsets only if ever explicitly asked); ping-pong = play MODE (meta/preview property, frames never duplicated). Inc 1 launches when the corridorkey-wiring worker frees canvas files.
- 2026-07-04: Lead UX decision: START keyframe = always required (the source art, auto); END keyframe = OPTIONAL slot -> absent = plain I2V (text drives), =start via Loop toggle = seamless cycle, different pose = FLF inbetween. Attack nuance explained+accepted: full game attack (idle->strike->idle) = 3 keyframes = two chained FLF segments = increment 3 piecewise.

---
id: T0255
title: "Canvas scale target: thousands of objects - tree memoization now; viewport culling + mutation payload diet next"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Culling landed 4fa7928b (my review + suite 540): conservative screen-AABB cull of elements (rotated = exact corner AABB; previewing/region-edit never culled) + group chrome (string-length pill bound, no measureText); clip-group offscreen = whole-subtree skip. Byte-identical screenshots at 3 viewports; 1024el grid: no regression all-visible, 2-4x zoomed. T0255 scope now: memoization DONE, culling DONE; payload diet = remaining (needs API contract discussion w/ lead).

---
id: T0209
title: "Polygonal regions: extensible shape model + editor (mine legacy impl)"
status: idea
project: P001
epic: E010
priority: P3
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-02: Future phase; scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02)
- 2026-07-02: Legacy system mined (report tmp/t0209_legacy_polygon_research_2026-07-02.md): polygon = rect + `polygon` [[x,y]...] >=3 pts (no shape field), slice = bbox crop + ImageDraw.polygon mask; editor = tool row select/rect/polygon, click-to-place vertices, dblclick/Enter closes, Ctrl+Z pops last vertex, bbox resize rescales points. Add-region UX decision: replace centered-rect button with Select/Draw Rect/Draw Polygon tool row - shape lands under the pointer. Do-not-port list and risks in the report.

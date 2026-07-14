---
id: T0209
title: "Polygonal regions: extensible shape model + editor (mine legacy impl)"
status: done
project: P001
epic: E010
priority: P3
tags: []
created: 2026-07-02
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=commit ead8f0998, polygon editor slicing and focused tests, full Studio CI run 29329533678 green"}]}
---

## What

Review and close the polygonal-region editor work after mining the legacy
implementation.

## Done when

- [x] Legacy polygon model and editor behavior are documented.
- [x] Polygon draw/select/slice behavior is implemented and validated.
- [ ] Remaining follow-ups, if any, are accepted or split into new tasks.

## Open questions

## Log
- 2026-07-02: Future phase; acceptance criteria in this task are the current
  scope.
- 2026-07-02: Legacy system mined (report tmp/t0209_legacy_polygon_research_2026-07-02.md): polygon = rect + `polygon` [[x,y]...] >=3 pts (no shape field), slice = bbox crop + ImageDraw.polygon mask; editor = tool row select/rect/polygon, click-to-place vertices, dblclick/Enter closes, Ctrl+Z pops last vertex, bbox resize rescales points. Add-region UX decision: replace centered-rect button with Select/Draw Rect/Draw Polygon tool row - shape lands under the pointer. Do-not-port list and risks in the report.
- 2026-07-02: SHIPPED ead8f099 (accepted: 87/87 tests incl. +10 polygon; live CDP verification). Tool row Select/Rect/Polygon in region-edit mode; polygon = rect + points, bbox auto-derived; slice masks alpha outside the polygon; draft Ctrl+Z pops vertices pre-journal. Not done (per research): per-vertex drag, rect<->polygon convert - follow-up on demand. Server restarted onto the new backend.
- 2026-07-14: Closure: waived; reason: grooming reconciled a stale historical checklist with the delivered or retained scope; evidence: commit ead8f0998, polygon editor slicing and focused tests
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=commit ead8f0998, polygon editor slicing and focused tests, full Studio CI run 29329533678 green

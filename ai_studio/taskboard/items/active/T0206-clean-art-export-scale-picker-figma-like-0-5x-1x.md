---
id: T0206
title: "Export panel in inspector (Figma-style): scale, format, quality, suffix, destination"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Replace context-menu export with a Figma-style Export section at the BOTTOM of
the inspector (lead 2026-07-02: "Давай export как в figme. Справа внизу, в
инспекторе... х размера и формат и сжатие"). Researched Figma behavior to
mirror: collapsible "Export" section, "+" adds export-setting rows; each row =
scale (0.5x/1x/2x/3x/4x or custom `2x`/`512w`/`512h`) + suffix (appended to
filename, e.g. @2x) + format dropdown; multiple rows per selection export in
one click; settings persist on the element; Export button labeled with the
selection; no selection -> project-level export (all screens). Our raster
adaptation: formats PNG (lossless) / JPG / WebP with a quality slider for the
lossy two (Figma hides quality; we expose it - the lead asked for "сжатие если
нужно"); resample filter choice (Lanczos smooth / nearest pixel-art) - the
anti-noise supersampling from the clean-art ladder lands here (generate 2x ->
export 1x). Destination (absorbed from T0203): the Export button asks WHERE via
the File System Access directory picker (Chrome), remembers the last folder per
project; fallback = browser download (zip when multiple files). Non-destructive:
originals in files/ never touched; scale=1 PNG stays the pure-copy path.
Parity: export settings stored on the element via journaled op; CLI
`export --scale --format --quality --suffix --to <dir>`; agent-driven exports
without --to keep writing to `<project>/export/<stamp>/`.

## Done when

- [x] inspector bottom has a collapsible Export section: rows of scale+suffix+format(+quality for JPG/WebP), "+" adds rows, settings persist per element (journaled, undoable) and survive reload
- [x] Export button exports all rows for the selection; multi-select exports each element; no selection exports the project screens
- [x] export asks destination (dir picker on EVERY export opening at the last-used folder — lead corrected to exact Figma behavior; download fallback only without FSA); CLI --to lands files at an explicit path; no --to keeps <project>/export/<stamp>/
- [x] custom scale syntax works (2x / 512w / 512h) with Lanczos/nearest filter choice; exported PNGs match requested dimensions; scale=1 PNG byte-identical copy
- [ ] generate-big -> export-small verified to visibly reduce AI noise on a real generated sheet (before/after in task log)
- [ ] lead verified the FSA picker flow live in Chrome (page-only plumbing untestable headless)
- [x] export removed from the context menu

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
- 2026-07-02: Rewritten after lead direction + Figma research: export moves from context menu to a Figma-style inspector section; absorbs export-destination scope from T0203; P2 -> P1.
- 2026-07-02: Lead hit the destination pain live ("экспорт идет непонятно куда, меня не спрашивает путь") - today export silently writes <project>/export/<stamp>/ with only a status-bar link. Confirms the ask-WHERE requirement; T0206 stays first in the canvas queue after the T0220 sweep.
- 2026-07-02: BUILT (deep-reasoner agent) + ACCEPTED, commit 6620d2a6. Ops: setExportSettings (journaled rows), exportElements evolved async (element x row, ONE export_images.py spawn per batch via _bridge venv, 1x-png byte-copy fast path), exportProject (visible groups via extracted compositeGroup), resolveExportScale (0.5x-4x/512w/512h). CLI export-set + export --to. Destination UX corrected by lead mid-build ("в figma каждый раз спрашивается папка, просто открывается последнее место"): picker EVERY export, startIn=remembered per-project IndexedDB handle, cancel aborts, no Change button, download fallback only when FSA absent. Design calls: no zip (real downloads per file); multi-select exports each element's own rows; jpg/webp always re-encode so quality applies. Gates: canvas 100/0, map strict, doc check. REMAINING: live Chrome check of the picker; supersampling before/after on a real generated sheet (2x->1x Lanczos path unit-verified).

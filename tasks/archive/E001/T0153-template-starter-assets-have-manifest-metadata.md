---
id: T0153
title: Template starter assets have manifest metadata
status: done
epic: E001
priority: P1
tags: [assets, template, licensing]
created: 2026-06-30
updated: 2026-06-30
---

## What

Template starter assets are currently visible as `unregistered` with
`license: unknown`, even though some have known provenance. Move template starter
assets into the same Pack Manifest metadata model used by global/game sources so
the viewer, search, and license guard see explicit provenance.

## Done when

- [x] Template UI images have manifest records with source/license/provenance.
- [x] Template cube has manifest metadata or a narrow documented exception.
- [x] Asset Viewer no longer shows template starter assets as unknown when they
      are known project-owned or CC0 assets.
- [x] License guard behavior is aligned with the manifest records.
- [x] Focused asset storage/viewer tests pass.

## Open questions

## Log

- 2026-06-30: Start: add Pack Manifest metadata for template starter assets and remove temporary allowlist coverage where manifest records can own provenance.
- 2026-06-30: Added template Pack Manifest records for `meshes/cube.glb` and the Kenney CC0 starter UI PNG files using source-root `source_resource/source_preview` paths, removed current allowlist entries, and verified template index shows 6 known CC0 assets instead of `unregistered/unknown`. Evidence: manifest tests 2/2, guard tests 16/16, current guard green, release guard green, full asset JS tests 105/105, Python prep tests 61/61, taskboard validate ok, architecture map validate ok.
- 2026-06-30: Start: add Pack Manifest metadata for template starter assets and remove temporary allowlist coverage where manifest records can own provenance.
- 2026-06-30: 2026-07-01: Review closeout passed on current state: asset JS tests 112/112, Python prep tests 65/65, restricted asset guard dev/release green, architecture map validation clean, taskboard validation clean, doc reference check clean, skills sync clean.

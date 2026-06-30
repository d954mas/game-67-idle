---
id: T0154
title: Asset license guard covers all public asset binaries
status: review
epic: E001
priority: P1
tags: [assets, licensing, guard]
created: 2026-06-30
updated: 2026-06-30
---

## What

The public-repo license guard must cover every committed binary asset under a
game/template `assets/` root, not only `assets/packs`, `assets/previews`, and
`assets/meshes`. Known UI/audio/font/model binaries must either resolve to a
manifest record, live under `assets/restricted/`, or be covered by a narrow
explicit allowlist.

## Done when

- [x] Guard blocks an unmanifested tracked binary under `assets/ui/`.
- [x] Guard keeps ignoring non-asset implementation files such as shaders.
- [x] Guard still supports per-game `assets/packs` and `assets/previews`.
- [x] Existing intentional allowlist entries remain narrow and documented.
- [x] Guard tests and current-tree guard pass.

## Open questions

## Log

- 2026-06-30: Start: broaden public git asset license guard from packs/previews/meshes-only to all binary files under game/template assets roots.
- 2026-06-30: Guard now audits every asset binary under `assets/`; added regression for `template/assets/ui/button.png` leak and shader ignore; narrowed allowlist to exact current template starter files. Evidence: guard unit tests 15/15, current-tree guard green, release guard green, full asset JS tests 103/103, taskboard validate ok, architecture map validate ok.
- 2026-06-30: Start: broaden public git asset license guard from packs/previews/meshes-only to all binary files under game/template assets roots.

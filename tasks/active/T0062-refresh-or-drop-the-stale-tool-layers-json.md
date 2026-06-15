---
id: T0062
title: Refresh or drop the stale tool_layers.json
status: backlog
epic: E003
priority: P3
tags: [docs, tooling, housekeeping]
created: 2026-06-15
updated: 2026-06-15
---

## What

`tools/tool_layers.json` is stale (omits ~14 live tools, disagrees with
`tools/README.md`). No code reads it programmatically -- the only references are
`tools/bootstrap/export_base.mjs:86` (lists the filename in the static copy
manifest; never parsed) and doc pointers in `tools/README.md:6` /
`AI_PIPELINE.md`. So it is a doc/data artifact shipped by the exporter, and it
currently misleads about the export/portability story.

## Done when

- [ ] Either refresh `tool_layers.json` to match the live tool set + `tools/README.md`, OR drop it and make `tools/README.md` the single source of truth for the tool map (and remove it from the export copy manifest + doc pointers if dropped).
- [ ] `node tools/pipeline_validate.mjs` + `node tools/taskboard/cli.mjs validate` pass; no dangling references if dropped.

## Open questions

- Refresh vs drop? Lean: drop and let `tools/README.md` be the map (one source of truth), unless the export needs a machine-readable layer manifest.

## Log

- 2026-06-15: Captured from the second simplification/speed iteration (doc-only stale; nothing reads it as logic).

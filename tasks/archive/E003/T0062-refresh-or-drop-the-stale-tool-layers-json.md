---
id: T0062
title: Refresh or drop the stale tool_layers.json
status: done
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

- [x] Dropped `tool_layers.json`; `tools/README.md` is now the single source of truth for the tool layers (its intro says so). Removed from `export_base.mjs` copy manifest and the two `AI_PIPELINE.md` pointers.
- [x] No dangling references; a real export validates (exported taskboard ok + skills_eval 9/9) without it; quick `pipeline_validate` passes.

## Open questions

- RESOLVED: dropped (not refreshed) -- one source of truth (README), nothing read it as logic.

## Log

- 2026-06-15: Captured from the second simplification/speed iteration (doc-only stale; nothing reads it as logic).
- 2026-06-15: Deleted tools/tool_layers.json; made tools/README.md the layer map; removed the export_base.mjs manifest entry + AI_PIPELINE.md pointers (:55, :391). Verified: real export self-validates without it, quick validate ok, no dangling refs.
